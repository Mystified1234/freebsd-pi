# $FreeBSD: projects/armv6/lib/clang/libllvmscalaropts/Makefile 234858 2012-05-01 04:01:22Z gonzo $

.include <bsd.own.mk>

LIB=	llvmscalaropts

SRCDIR=	lib/Transforms/Scalar
SRCS=	ADCE.cpp \
	BasicBlockPlacement.cpp \
	CodeGenPrepare.cpp \
	ConstantProp.cpp \
	CorrelatedValuePropagation.cpp \
	DCE.cpp \
	DeadStoreElimination.cpp \
	EarlyCSE.cpp \
	GlobalMerge.cpp \
	GVN.cpp \
	IndVarSimplify.cpp \
	JumpThreading.cpp \
	LICM.cpp \
	LoopDeletion.cpp \
	LoopIdiomRecognize.cpp \
	LoopRotation.cpp \
	LoopStrengthReduce.cpp \
	LoopUnrollPass.cpp \
	LoopUnswitch.cpp \
	LowerAtomic.cpp \
	MemCpyOptimizer.cpp \
	ObjCARC.cpp \
	Reassociate.cpp \
	Reg2Mem.cpp \
	SCCP.cpp \
	ScalarReplAggregates.cpp \
	SimplifyCFGPass.cpp \
	SimplifyLibCalls.cpp \
	Sink.cpp \
	TailRecursionElimination.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	LoopInstSimplify.cpp \
	LowerAtomic.cpp \
	Reg2Mem.cpp \
	Scalar.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
