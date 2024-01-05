#   osmodel.mak
#
# Detects and sets the macros:
#
#   OS         = linux
#   MODEL      = one of { 32, 64 }
#   MODEL_FLAG = one of { -m32, -m64 }
#
# Note:
#   Keep this file in sync between druntime, phobos, and dmd repositories!
# Source: https://github.com/dlang/dmd/blob/master/osmodel.mak

ifeq (,$(OS))
  uname_S:=$(shell uname -s)
  ifeq (Linux,$(uname_S))
    OS:=linux
  endif
  ifeq (,$(OS))
    $(error Unrecognized or unsupported OS for uname: $(uname_S))
  endif
endif

ifeq (,$(MODEL))
    uname_M:=$(shell uname -m)
  ifneq (,$(findstring $(uname_M),x86_64 amd64))
    MODEL:=64
  endif
  ifneq (,$(findstring $(uname_M),i386 i586 i686))
    MODEL:=32
  endif
  ifeq (,$(MODEL))
    $(error Cannot figure 32/64 model from uname -m: $(uname_M))
  endif
endif

MODEL_FLAG:=-m$(MODEL)
