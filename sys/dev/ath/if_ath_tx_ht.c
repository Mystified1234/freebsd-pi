/*-
 * Copyright (c) 2011 Adrian Chadd, Xenion Pty Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: projects/armv6/sys/dev/ath/if_ath_tx_ht.c 234858 2012-05-01 04:01:22Z gonzo $");

#include "opt_inet.h"
#include "opt_ath.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#ifdef IEEE80211_SUPPORT_TDMA
#include <net80211/ieee80211_tdma.h>
#endif

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_hal/ah_devid.h>		/* XXX for softled */
#include <dev/ath/ath_hal/ah_diagcodes.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#include <dev/ath/if_ath_tx.h>		/* XXX for some support functions */
#include <dev/ath/if_ath_tx_ht.h>
#include <dev/ath/if_athrate.h>
#include <dev/ath/if_ath_debug.h>

/*
 * XXX net80211?
 */
#define	IEEE80211_AMPDU_SUBFRAME_DEFAULT		32

#define	ATH_AGGR_DELIM_SZ	4	/* delimiter size   */
#define	ATH_AGGR_MINPLEN	256	/* in bytes, minimum packet length */
#define	ATH_AGGR_ENCRYPTDELIM	10	/* number of delimiters for encryption padding */

/*
 * returns delimiter padding required given the packet length
 */
#define	ATH_AGGR_GET_NDELIM(_len)					\
	    (((((_len) + ATH_AGGR_DELIM_SZ) < ATH_AGGR_MINPLEN) ?	\
	    (ATH_AGGR_MINPLEN - (_len) - ATH_AGGR_DELIM_SZ) : 0) >> 2)

#define	PADBYTES(_len)		((4 - ((_len) % 4)) % 4)

int ath_max_4ms_framelen[4][32] = {
	[MCS_HT20] = {
		3212,  6432,  9648,  12864,  19300,  25736,  28952,  32172,
		6424,  12852, 19280, 25708,  38568,  51424,  57852,  64280,
		9628,  19260, 28896, 38528,  57792,  65532,  65532,  65532,
		12828, 25656, 38488, 51320,  65532,  65532,  65532,  65532,
	},
	[MCS_HT20_SGI] = {
		3572,  7144,  10720,  14296,  21444,  28596,  32172,  35744,
		7140,  14284, 21428,  28568,  42856,  57144,  64288,  65532,
		10700, 21408, 32112,  42816,  64228,  65532,  65532,  65532,
		14256, 28516, 42780,  57040,  65532,  65532,  65532,  65532,
	},
	[MCS_HT40] = {
		6680,  13360,  20044,  26724,  40092,  53456,  60140,  65532,
		13348, 26700,  40052,  53400,  65532,  65532,  65532,  65532,
		20004, 40008,  60016,  65532,  65532,  65532,  65532,  65532,
		26644, 53292,  65532,  65532,  65532,  65532,  65532,  65532,
	},
	[MCS_HT40_SGI] = {
		7420,  14844,  22272,  29696,  44544,  59396,  65532,  65532,
		14832, 29668,  44504,  59340,  65532,  65532,  65532,  65532,
		22232, 44464,  65532,  65532,  65532,  65532,  65532,  65532,
		29616, 59232,  65532,  65532,  65532,  65532,  65532,  65532,
	}
};

/*
 * XXX should be in net80211
 */
static int ieee80211_mpdudensity_map[] = {
	0,		/* IEEE80211_HTCAP_MPDUDENSITY_NA */
	25,		/* IEEE80211_HTCAP_MPDUDENSITY_025 */
	50,		/* IEEE80211_HTCAP_MPDUDENSITY_05 */
	100,		/* IEEE80211_HTCAP_MPDUDENSITY_1 */
	200,		/* IEEE80211_HTCAP_MPDUDENSITY_2 */
	400,		/* IEEE80211_HTCAP_MPDUDENSITY_4 */
	800,		/* IEEE80211_HTCAP_MPDUDENSITY_8 */
	1600,		/* IEEE80211_HTCAP_MPDUDENSITY_16 */
};

/*
 * XXX should be in the HAL/net80211 ?
 */
#define	BITS_PER_BYTE		8
#define	OFDM_PLCP_BITS		22
#define	HT_RC_2_MCS(_rc)	((_rc) & 0x7f)
#define	HT_RC_2_STREAMS(_rc)	((((_rc) & 0x78) >> 3) + 1)
#define	L_STF			8
#define	L_LTF			8
#define	L_SIG			4
#define	HT_SIG			8
#define	HT_STF			4
#define	HT_LTF(_ns)		(4 * (_ns))
#define	SYMBOL_TIME(_ns)	((_ns) << 2)		// ns * 4 us
#define	SYMBOL_TIME_HALFGI(_ns)	(((_ns) * 18 + 4) / 5)	// ns * 3.6 us
#define	NUM_SYMBOLS_PER_USEC(_usec)	(_usec >> 2)
#define	NUM_SYMBOLS_PER_USEC_HALFGI(_usec)	(((_usec*5)-4)/18)
#define	IS_HT_RATE(_rate)	((_rate) & 0x80)

const uint32_t bits_per_symbol[][2] = {
    /* 20MHz 40MHz */
    {    26,   54 },     //  0: BPSK
    {    52,  108 },     //  1: QPSK 1/2
    {    78,  162 },     //  2: QPSK 3/4
    {   104,  216 },     //  3: 16-QAM 1/2
    {   156,  324 },     //  4: 16-QAM 3/4
    {   208,  432 },     //  5: 64-QAM 2/3
    {   234,  486 },     //  6: 64-QAM 3/4
    {   260,  540 },     //  7: 64-QAM 5/6
    {    52,  108 },     //  8: BPSK
    {   104,  216 },     //  9: QPSK 1/2
    {   156,  324 },     // 10: QPSK 3/4
    {   208,  432 },     // 11: 16-QAM 1/2
    {   312,  648 },     // 12: 16-QAM 3/4
    {   416,  864 },     // 13: 64-QAM 2/3
    {   468,  972 },     // 14: 64-QAM 3/4
    {   520, 1080 },     // 15: 64-QAM 5/6
    {    78,  162 },     // 16: BPSK
    {   156,  324 },     // 17: QPSK 1/2
    {   234,  486 },     // 18: QPSK 3/4
    {   312,  648 },     // 19: 16-QAM 1/2
    {   468,  972 },     // 20: 16-QAM 3/4
    {   624, 1296 },     // 21: 64-QAM 2/3
    {   702, 1458 },     // 22: 64-QAM 3/4
    {   780, 1620 },     // 23: 64-QAM 5/6
    {   104,  216 },     // 24: BPSK
    {   208,  432 },     // 25: QPSK 1/2
    {   312,  648 },     // 26: QPSK 3/4
    {   416,  864 },     // 27: 16-QAM 1/2
    {   624, 1296 },     // 28: 16-QAM 3/4
    {   832, 1728 },     // 29: 64-QAM 2/3
    {   936, 1944 },     // 30: 64-QAM 3/4
    {  1040, 2160 },     // 31: 64-QAM 5/6
};

/*
 * Fill in the rate array information based on the current
 * node configuration and the choices made by the rate
 * selection code and ath_buf setup code.
 *
 * Later on, this may end up also being made by the
 * rate control code, but for now it can live here.
 *
 * This needs to be called just before the packet is
 * queued to the software queue or hardware queue,
 * so all of the needed fields in bf_state are setup.
 */
void
ath_tx_rate_fill_rcflags(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ieee80211com *ic = ni->ni_ic;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	struct ath_rc_series *rc = bf->bf_state.bfs_rc;
	uint8_t rate;
	int i;

	for (i = 0; i < ATH_RC_NUM; i++) {
		rc[i].flags = 0;
		if (rc[i].tries == 0)
			continue;

		rate = rt->info[rc[i].rix].rateCode;

		/*
		 * XXX only do this for legacy rates?
		 */
		if (bf->bf_state.bfs_shpream)
			rate |= rt->info[rc[i].rix].shortPreamble;

		/*
		 * Save this, used by the TX and completion code
		 */
		rc[i].ratecode = rate;

		if (bf->bf_state.bfs_txflags &
		    (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA))
			rc[i].flags |= ATH_RC_RTSCTS_FLAG;

		/* Only enable shortgi, 2040, dual-stream if HT is set */
		if (IS_HT_RATE(rate)) {
			rc[i].flags |= ATH_RC_HT_FLAG;

			if (ni->ni_chw == 40)
				rc[i].flags |= ATH_RC_CW40_FLAG;

			if (ni->ni_chw == 40 &&
			    ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI40 &&
			    ni->ni_htcap & IEEE80211_HTCAP_SHORTGI40)
				rc[i].flags |= ATH_RC_SGI_FLAG;

			if (ni->ni_chw == 20 &&
			    ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI20 &&
			    ni->ni_htcap & IEEE80211_HTCAP_SHORTGI20)
				rc[i].flags |= ATH_RC_SGI_FLAG;

			/* XXX dual stream? and 3-stream? */
		}

		/*
		 * Calculate the maximum 4ms frame length based
		 * on the MCS rate, SGI and channel width flags.
		 */
		if ((rc[i].flags & ATH_RC_HT_FLAG) &&
		    (HT_RC_2_MCS(rate) < 32)) {
			int j;
			if (rc[i].flags & ATH_RC_CW40_FLAG) {
				if (rc[i].flags & ATH_RC_SGI_FLAG)
					j = MCS_HT40_SGI;
				else
					j = MCS_HT40;
			} else {
				if (rc[i].flags & ATH_RC_SGI_FLAG)
					j = MCS_HT20_SGI;
				else
					j = MCS_HT20;
			}
			rc[i].max4msframelen =
			    ath_max_4ms_framelen[j][HT_RC_2_MCS(rate)];
		} else
			rc[i].max4msframelen = 0;
		DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
		    "%s: i=%d, rate=0x%x, flags=0x%x, max4ms=%d\n",
		    __func__, i, rate, rc[i].flags, rc[i].max4msframelen);
	}
}

/*
 * Return the number of delimiters to be added to
 * meet the minimum required mpdudensity.
 *
 * Caller should make sure that the rate is HT.
 *
 * TODO: is this delimiter calculation supposed to be the
 * total frame length, the hdr length, the data length (including
 * delimiters, padding, CRC, etc) or ?
 *
 * TODO: this should ensure that the rate control information
 * HAS been setup for the first rate.
 *
 * TODO: ensure this is only called for MCS rates.
 *
 * TODO: enforce MCS < 31
 */
static int
ath_compute_num_delims(struct ath_softc *sc, struct ath_buf *first_bf,
    uint16_t pktlen)
{
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	struct ieee80211_node *ni = first_bf->bf_node;
	struct ieee80211vap *vap = ni->ni_vap;
	int ndelim, mindelim = 0;
	int mpdudensity;	 /* in 1/100'th of a microsecond */
	uint8_t rc, rix, flags;
	int width, half_gi;
	uint32_t nsymbits, nsymbols;
	uint16_t minlen;

	/*
	 * vap->iv_ampdu_density is a value, rather than the actual
	 * density.
	 */
	if (vap->iv_ampdu_density > IEEE80211_HTCAP_MPDUDENSITY_16)
		mpdudensity = 1600;		/* maximum density */
	else
		mpdudensity = ieee80211_mpdudensity_map[vap->iv_ampdu_density];

	/* Select standard number of delimiters based on frame length */
	ndelim = ATH_AGGR_GET_NDELIM(pktlen);

	/*
	 * If encryption is enabled, add extra delimiters to let the
	 * crypto hardware catch up. This could be tuned per-MAC and
	 * per-rate, but for now we'll simply assume encryption is
	 * always enabled.
	 */
	ndelim += ATH_AGGR_ENCRYPTDELIM;

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
	    "%s: pktlen=%d, ndelim=%d, mpdudensity=%d\n",
	    __func__, pktlen, ndelim, mpdudensity);

	/*
	 * If the MPDU density is 0, we can return here.
	 * Otherwise, we need to convert the desired mpdudensity
	 * into a byte length, based on the rate in the subframe.
	 */
	if (mpdudensity == 0)
		return ndelim;

	/*
	 * Convert desired mpdu density from microeconds to bytes based
	 * on highest rate in rate series (i.e. first rate) to determine
	 * required minimum length for subframe. Take into account
	 * whether high rate is 20 or 40Mhz and half or full GI.
	 */
	rix = first_bf->bf_state.bfs_rc[0].rix;
	rc = rt->info[rix].rateCode;
	flags = first_bf->bf_state.bfs_rc[0].flags;
	width = !! (flags & ATH_RC_CW40_FLAG);
	half_gi = !! (flags & ATH_RC_SGI_FLAG);

	/*
	 * mpdudensity is in 1/100th of a usec, so divide by 100
	 */
	if (half_gi)
		nsymbols = NUM_SYMBOLS_PER_USEC_HALFGI(mpdudensity);
	else
		nsymbols = NUM_SYMBOLS_PER_USEC(mpdudensity);
	nsymbols /= 100;

	if (nsymbols == 0)
		nsymbols = 1;

	nsymbits = bits_per_symbol[HT_RC_2_MCS(rc)][width];
	minlen = (nsymbols * nsymbits) / BITS_PER_BYTE;

	/*
	 * Min length is the minimum frame length for the
	 * required MPDU density.
	 */
	if (pktlen < minlen) {
		mindelim = (minlen - pktlen) / ATH_AGGR_DELIM_SZ;
		ndelim = MAX(mindelim, ndelim);
	}

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
	    "%s: pktlen=%d, minlen=%d, rix=%x, rc=%x, width=%d, hgi=%d, ndelim=%d\n",
	    __func__, pktlen, minlen, rix, rc, width, half_gi, ndelim);

	return ndelim;
}

/*
 * Fetch the aggregation limit.
 *
 * It's the lowest of the four rate series 4ms frame length.
 */
static int
ath_get_aggr_limit(struct ath_softc *sc, struct ath_buf *bf)
{
	int amin = 65530;
	int i;

	for (i = 0; i < 4; i++) {
		if (bf->bf_state.bfs_rc[i].tries == 0)
			continue;
		amin = MIN(amin, bf->bf_state.bfs_rc[i].max4msframelen);
	}

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR, "%s: max frame len= %d\n",
	    __func__, amin);

	return amin;
}

/*
 * Setup a 11n rate series structure
 *
 * This should be called for both legacy and MCS rates.
 *
 * It, along with ath_buf_set_rate, must be called -after- a burst
 * or aggregate is setup.
 */
static void
ath_rateseries_setup(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_buf *bf, HAL_11N_RATE_SERIES *series)
{
#define	HT_RC_2_STREAMS(_rc)	((((_rc) & 0x78) >> 3) + 1)
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_hal *ah = sc->sc_ah;
	HAL_BOOL shortPreamble = AH_FALSE;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	int i;
	int pktlen;
	int flags = bf->bf_state.bfs_txflags;
	struct ath_rc_series *rc = bf->bf_state.bfs_rc;

	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
		shortPreamble = AH_TRUE;

	/*
	 * If this is the first frame in an aggregate series,
	 * use the aggregate length.
	 */
	if (bf->bf_state.bfs_aggr)
		pktlen = bf->bf_state.bfs_al;
	else
		pktlen = bf->bf_state.bfs_pktlen;

	/*
	 * XXX TODO: modify this routine to use the bfs_rc[x].flags
	 * XXX fields.
	 */
	memset(series, 0, sizeof(HAL_11N_RATE_SERIES) * 4);
	for (i = 0; i < 4;  i++) {
		/* Only set flags for actual TX attempts */
		if (rc[i].tries == 0)
			continue;

		series[i].Tries = rc[i].tries;

		/*
		 * XXX this isn't strictly correct - sc_txchainmask
		 * XXX isn't the currently active chainmask;
		 * XXX it's the interface chainmask at startup.
		 * XXX It's overridden in the HAL rate scenario function
		 * XXX for now.
		 */
		series[i].ChSel = sc->sc_txchainmask;

		if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA))
			series[i].RateFlags |= HAL_RATESERIES_RTS_CTS;

		/*
		 * Transmit 40MHz frames only if the node has negotiated
		 * it rather than whether the node is capable of it or not.
	 	 * It's subtly different in the hostap case.
	 	 */
		if (ni->ni_chw == 40)
			series[i].RateFlags |= HAL_RATESERIES_2040;

		/*
		 * Set short-GI only if the node has advertised it
		 * the channel width is suitable, and we support it.
		 * We don't currently have a "negotiated" set of bits -
		 * ni_htcap is what the remote end sends, not what this
		 * node is capable of.
		 */
		if (ni->ni_chw == 40 &&
		    ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI40 &&
		    ni->ni_htcap & IEEE80211_HTCAP_SHORTGI40)
			series[i].RateFlags |= HAL_RATESERIES_HALFGI;

		if (ni->ni_chw == 20 &&
		    ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI20 &&
		    ni->ni_htcap & IEEE80211_HTCAP_SHORTGI20)
			series[i].RateFlags |= HAL_RATESERIES_HALFGI;

		series[i].Rate = rt->info[rc[i].rix].rateCode;

		/* PktDuration doesn't include slot, ACK, RTS, etc timing - it's just the packet duration */
		if (series[i].Rate & IEEE80211_RATE_MCS) {
			series[i].PktDuration =
			    ath_computedur_ht(pktlen
				, series[i].Rate
				, HT_RC_2_STREAMS(series[i].Rate)
				, series[i].RateFlags & HAL_RATESERIES_2040
				, series[i].RateFlags & HAL_RATESERIES_HALFGI);
		} else {
			if (shortPreamble)
				series[i].Rate |=
				    rt->info[rc[i].rix].shortPreamble;
			series[i].PktDuration = ath_hal_computetxtime(ah,
			    rt, pktlen, rc[i].rix, shortPreamble);
		}
	}
#undef	HT_RC_2_STREAMS
}

#if 0
static void
ath_rateseries_print(HAL_11N_RATE_SERIES *series)
{
	int i;
	for (i = 0; i < 4; i++) {
		printf("series %d: rate %x; tries %d; pktDuration %d; chSel %d; rateFlags %x\n",
		    i,
		    series[i].Rate,
		    series[i].Tries,
		    series[i].PktDuration,
		    series[i].ChSel,
		    series[i].RateFlags);
	}
}
#endif

/*
 * Setup the 11n rate scenario and burst duration for the given TX descriptor
 * list.
 *
 * This isn't useful for sending beacon frames, which has different needs
 * wrt what's passed into the rate scenario function.
 */

void
ath_buf_set_rate(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_buf *bf)
{
	HAL_11N_RATE_SERIES series[4];
	struct ath_desc *ds = bf->bf_desc;
	struct ath_desc *lastds = NULL;
	struct ath_hal *ah = sc->sc_ah;
	int is_pspoll = (bf->bf_state.bfs_atype == HAL_PKT_TYPE_PSPOLL);
	int ctsrate = bf->bf_state.bfs_ctsrate;
	int flags = bf->bf_state.bfs_txflags;

	/* Setup rate scenario */
	memset(&series, 0, sizeof(series));

	ath_rateseries_setup(sc, ni, bf, series);

	/* Enforce AR5416 aggregate limit - can't do RTS w/ an agg frame > 8k */

	/* Enforce RTS and CTS are mutually exclusive */

	/* Get a pointer to the last tx descriptor in the list */
	lastds = bf->bf_lastds;

#if 0
	printf("pktlen: %d; flags 0x%x\n", pktlen, flags);
	ath_rateseries_print(series);
#endif

	/* Set rate scenario */
	ath_hal_set11nratescenario(ah, ds,
	    !is_pspoll,	/* whether to override the duration or not */
			/* don't allow hardware to override the duration on ps-poll packets */
	    ctsrate,	/* rts/cts rate */
	    series,	/* 11n rate series */
	    4,		/* number of series */
	    flags);

	/* Setup the last descriptor in the chain */
	ath_hal_setuplasttxdesc(ah, lastds, ds);

	/* Set burst duration */
	/*
	 * This is only required when doing 11n burst, not aggregation
	 * ie, if there's a second frame in a RIFS or A-MPDU burst
	 * w/ >1 A-MPDU frame bursting back to back.
	 * Normal A-MPDU doesn't do bursting -between- aggregates.
	 *
	 * .. and it's highly likely this won't ever be implemented
	 */
	//ath_hal_set11nburstduration(ah, ds, 8192);
}

/*
 * Form an aggregate packet list.
 *
 * This function enforces the aggregate restrictions/requirements.
 *
 * These are:
 *
 * + The aggregate size maximum (64k for AR9160 and later, 8K for
 *   AR5416 when doing RTS frame protection.)
 * + Maximum number of sub-frames for an aggregate
 * + The aggregate delimiter size, giving MACs time to do whatever is
 *   needed before each frame
 * + Enforce the BAW limit
 *
 * Each descriptor queued should have the DMA setup.
 * The rate series, descriptor setup, linking, etc is all done
 * externally. This routine simply chains them together.
 * ath_tx_setds_11n() will take care of configuring the per-
 * descriptor setup, and ath_buf_set_rate() will configure the
 * rate control.
 *
 * Note that the TID lock is only grabbed when dequeuing packets from
 * the TID queue. If some code in another thread adds to the head of this
 * list, very strange behaviour will occur. Since retransmission is the
 * only reason this will occur, and this routine is designed to be called
 * from within the scheduler task, it won't ever clash with the completion
 * task.
 *
 * So if you want to call this from an upper layer context (eg, to direct-
 * dispatch aggregate frames to the hardware), please keep this in mind.
 */
ATH_AGGR_STATUS
ath_tx_form_aggr(struct ath_softc *sc, struct ath_node *an, struct ath_tid *tid,
    ath_bufhead *bf_q)
{
	struct ieee80211_node *ni = &an->an_node;
	struct ath_buf *bf, *bf_first = NULL, *bf_prev = NULL;
	int nframes = 0;
	uint16_t aggr_limit = 0, al = 0, bpad = 0, al_delta, h_baw;
	struct ieee80211_tx_ampdu *tap;
	int status = ATH_AGGR_DONE;
	int prev_frames = 0;	/* XXX for AR5416 burst, not done here */
	int prev_al = 0;	/* XXX also for AR5416 burst */

	ATH_TXQ_LOCK_ASSERT(sc->sc_ac2q[tid->ac]);

	tap = ath_tx_get_tx_tid(an, tid->tid);
	if (tap == NULL) {
		status = ATH_AGGR_ERROR;
		goto finish;
	}

	h_baw = tap->txa_wnd / 2;

	for (;;) {
		bf = TAILQ_FIRST(&tid->axq_q);
		if (bf_first == NULL)
			bf_first = bf;
		if (bf == NULL) {
			status = ATH_AGGR_DONE;
			break;
		} else {
			/*
			 * It's the first frame;
			 * set the aggregation limit based on the
			 * rate control decision that has been made.
			 */
			aggr_limit = ath_get_aggr_limit(sc, bf_first);
		}

		/* Set this early just so things don't get confused */
		bf->bf_next = NULL;

		/*
		 * Don't unlock the tid lock until we're sure we are going
		 * to queue this frame.
		 */

		/*
		 * If the frame doesn't have a sequence number that we're
		 * tracking in the BAW (eg NULL QOS data frame), we can't
		 * aggregate it. Stop the aggregation process; the sender
		 * can then TX what's in the list thus far and then
		 * TX the frame individually.
		 */
		if (! bf->bf_state.bfs_dobaw) {
			status = ATH_AGGR_NONAGGR;
			break;
		}

		/*
		 * If any of the rates are non-HT, this packet
		 * can't be aggregated.
		 * XXX TODO: add a bf_state flag which gets marked
		 * if any active rate is non-HT.
		 */

		/*
		 * do not exceed aggregation limit
		 */
		al_delta = ATH_AGGR_DELIM_SZ + bf->bf_state.bfs_pktlen;
		if (nframes &&
		    (aggr_limit < (al + bpad + al_delta + prev_al))) {
			status = ATH_AGGR_LIMITED;
			break;
		}

		/*
		 * If RTS/CTS is set on the first frame, enforce
		 * the RTS aggregate limit.
		 */
		if (bf_first->bf_state.bfs_txflags &
		    (HAL_TXDESC_CTSENA | HAL_TXDESC_RTSENA)) {
			if (nframes &&
			   (sc->sc_rts_aggr_limit <
			     (al + bpad + al_delta + prev_al))) {
				status = ATH_AGGR_8K_LIMITED;
				break;
			}
		}

		/*
		 * Do not exceed subframe limit.
		 */
		if ((nframes + prev_frames) >= MIN((h_baw),
		    IEEE80211_AMPDU_SUBFRAME_DEFAULT)) {
			status = ATH_AGGR_LIMITED;
			break;
		}

		/*
		 * If the current frame has an RTS/CTS configuration
		 * that differs from the first frame, override the
		 * subsequent frame with this config.
		 */
		bf->bf_state.bfs_txflags &=
		    (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA);
		bf->bf_state.bfs_txflags |=
		    bf_first->bf_state.bfs_txflags &
		    (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA);

		/*
		 * TODO: If it's _before_ the BAW left edge, complain very
		 * loudly.
		 *
		 * This means something (else) has slid the left edge along
		 * before we got a chance to be TXed.
		 */

		/*
		 * Check if we have space in the BAW for this frame before
		 * we add it.
		 *
		 * see ath_tx_xmit_aggr() for more info.
		 */
		if (bf->bf_state.bfs_dobaw) {
			ieee80211_seq seqno;

			/*
			 * If the sequence number is allocated, use it.
			 * Otherwise, use the sequence number we WOULD
			 * allocate.
			 */
			if (bf->bf_state.bfs_seqno_assigned)
				seqno = SEQNO(bf->bf_state.bfs_seqno);
			else
				seqno = ni->ni_txseqs[bf->bf_state.bfs_tid];

			/*
			 * Check whether either the currently allocated
			 * sequence number _OR_ the to-be allocated
			 * sequence number is inside the BAW.
			 */
			if (! BAW_WITHIN(tap->txa_start, tap->txa_wnd,
			    seqno)) {
				status = ATH_AGGR_BAW_CLOSED;
				break;
			}

			/* XXX check for bfs_need_seqno? */
			if (! bf->bf_state.bfs_seqno_assigned) {
				int seqno;
				seqno = ath_tx_tid_seqno_assign(sc, ni, bf, bf->bf_m);
				if (seqno < 0) {
					device_printf(sc->sc_dev,
					    "%s: bf=%p, huh, seqno=-1?\n",
					    __func__,
					    bf);
					/* XXX what can we even do here? */
				}
				/* Flush seqno update to RAM */
				/*
				 * XXX This is required because the dmasetup
				 * XXX is done early rather than at dispatch
				 * XXX time. Ew, we should fix this!
				 */
				bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
				    BUS_DMASYNC_PREWRITE);
			}
		}

		/*
		 * If the packet has a sequence number, do not
		 * step outside of the block-ack window.
		 */
		if (! BAW_WITHIN(tap->txa_start, tap->txa_wnd,
		    SEQNO(bf->bf_state.bfs_seqno))) {
			device_printf(sc->sc_dev,
			    "%s: bf=%p, seqno=%d, outside?!\n",
			    __func__, bf, SEQNO(bf->bf_state.bfs_seqno));
			status = ATH_AGGR_BAW_CLOSED;
			break;
		}

		/*
		 * this packet is part of an aggregate.
		 */
		ATH_TXQ_REMOVE(tid, bf, bf_list);

		/* The TID lock is required for the BAW update */
		ath_tx_addto_baw(sc, an, tid, bf);
		bf->bf_state.bfs_addedbaw = 1;

		/*
		 * XXX enforce ACK for aggregate frames (this needs to be
		 * XXX handled more gracefully?
		 */
		if (bf->bf_state.bfs_txflags & HAL_TXDESC_NOACK) {
			device_printf(sc->sc_dev,
			    "%s: HAL_TXDESC_NOACK set for an aggregate frame?\n",
			    __func__);
			bf->bf_state.bfs_txflags &= (~HAL_TXDESC_NOACK);
		}

		/*
		 * Add the now owned buffer (which isn't
		 * on the software TXQ any longer) to our
		 * aggregate frame list.
		 */
		TAILQ_INSERT_TAIL(bf_q, bf, bf_list);
		nframes ++;

		/* Completion handler */
		bf->bf_comp = ath_tx_aggr_comp;

		/*
		 * add padding for previous frame to aggregation length
		 */
		al += bpad + al_delta;

		/*
		 * Calculate delimiters needed for the current frame
		 */
		bf->bf_state.bfs_ndelim =
		    ath_compute_num_delims(sc, bf_first,
		    bf->bf_state.bfs_pktlen);

		/*
		 * Calculate the padding needed from this set of delimiters,
		 * used when calculating if the next frame will fit in
		 * the aggregate.
		 */
		bpad = PADBYTES(al_delta) + (bf->bf_state.bfs_ndelim << 2);

		/*
		 * Chain the buffers together
		 */
		if (bf_prev)
			bf_prev->bf_next = bf;
		bf_prev = bf;

		/*
		 * XXX TODO: if any sub-frames have RTS/CTS enabled;
		 * enable it for the entire aggregate.
		 */

#if 0
		/*
		 * terminate aggregation on a small packet boundary
		 */
		if (bf->bf_state.bfs_pktlen < ATH_AGGR_MINPLEN) {
			status = ATH_AGGR_SHORTPKT;
			break;
		}
#endif

	}

finish:
	/*
	 * Just in case the list was empty when we tried to
	 * dequeue a packet ..
	 */
	if (bf_first) {
		bf_first->bf_state.bfs_al = al;
		bf_first->bf_state.bfs_nframes = nframes;
	}
	return status;
}
