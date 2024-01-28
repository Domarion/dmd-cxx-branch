# get OS and MODEL
include osmodel.mak

ifeq (,$(TARGET_CPU))
    $(info no cpu specified, assuming X86)
    TARGET_CPU=X86
endif

ifeq (X86,$(TARGET_CPU))
    TARGET_CH = $C/code_x86.hpp
    TARGET_OBJS = cg87.o cgxmm.o cgsched.o cod1.o cod2.o cod3.o cod4.o ptrntab.o
else
    ifeq (stub,$(TARGET_CPU))
        TARGET_CH = $C/code_stub.hpp
        TARGET_OBJS = platform_stub.o
    else
        $(error unknown TARGET_CPU: '$(TARGET_CPU)')
    endif
endif

# default to PIC on x86_64, use PIC=1/0 to en-/disable PIC.
# Note that shared libraries and C files are always compiled with PIC.
ifeq ($(PIC),)
    ifeq ($(MODEL),64) # x86_64
        PIC:=1
    else
        PIC:=0
    endif
endif
ifeq ($(PIC),1)
    override PIC:=-fPIC
else
    override PIC:=
endif

INSTALL_DIR=../../install
# can be set to override the default /etc/
SYSCONFDIR=/etc/
PGO_DIR=$(abspath pgo)

C=backend
TK=tk
ROOT=root

GENERATED = ../generated
BUILD=release
G = $(GENERATED)/$(OS)/$(BUILD)/$(MODEL)
$(shell mkdir -p $G)

LDFLAGS=-lm -lstdc++ -lpthread

HOST_CXX=c++
# compatibility with old behavior
ifneq ($(HOST_CC),)
  $(warning ===== WARNING: Please use HOST_CXX=$(HOST_CC) instead of HOST_CC=$(HOST_CC). =====)
  HOST_CXX=$(HOST_CC)
endif
CXX=$(HOST_CXX)
AR=ar
GIT=git

# determine whether CXX is gcc or clang based
CXX_VERSION:=$(shell $(CXX) --version)
ifneq (,$(findstring g++,$(CXX_VERSION))$(findstring gcc,$(CXX_VERSION))$(findstring Free Software,$(CXX_VERSION)))
	CXX_KIND=g++
endif
ifneq (,$(findstring clang,$(CXX_VERSION)))
	CXX_KIND=clang++
endif

# Compiler Warnings
ifdef ENABLE_WARNINGS
WARNINGS := -Wall -Wextra -pedantic\
	-Wwrite-strings \
	-Wno-long-long \
	-Wno-variadic-macros -Wredundant-decls -Wreturn-type \
	-Wno-overlength-strings -Wfloat-equal -Wmissing-field-initializers \
	-Wsequence-point -Wshadow -Wstrict-aliasing -Wstrict-aliasing=2 \
	-Wunreachable-code -Wunused -Wunused-function -Wunused-label -Wunused-parameter
# Frontend specific
DMD_WARNINGS := -Wcast-qual \
	-Wuninitialized
ROOT_WARNINGS := -Wno-sign-compare \
	-Wno-unused-parameter
# Backend specific
GLUE_WARNINGS := $(ROOT_WARNINGS) \
	-Wno-format \
	-Wno-parentheses \
	-Wno-switch \
	-Wno-unused-function \
	-Wno-unused-variable
BACK_WARNINGS := $(GLUE_WARNINGS) \
	-Wno-char-subscripts \
	-Wno-empty-body \
	-Wno-missing-field-initializers \
	-Wno-type-limits \
	-Wno-unused-label \
	-Wno-unused-value \
	-Wno-varargs
# GCC Specific
ifeq ($(CXX_KIND), g++)
BACK_WARNINGS += \
	-Wno-unused-but-set-variable \
	-Wno-implicit-fallthrough \
	-Wno-class-memaccess \
	-Wno-uninitialized
endif
# Clang Specific
ifeq ($(CXX_KIND), clang++)
WARNINGS += \
	-Wno-undefined-var-template \
	-Wno-absolute-value \
	-Wno-missing-braces \
	-Wno-self-assign \
	-Wno-unused-const-variable \
	-Wno-constant-conversion \
	-Wno-overloaded-virtual
endif
else
# Default Warnings
WARNINGS := -Wno-deprecated -Wstrict-aliasing
# Frontend specific
DMD_WARNINGS := -Wuninitialized
ROOT_WARNINGS :=
# Backend specific
GLUE_WARNINGS := $(ROOT_WARNINGS) \
	-Wno-switch
BACK_WARNINGS := $(GLUE_WARNINGS) \
	-Wno-unused-value \
	-Wno-varargs
# Clang Specific
ifeq ($(CXX_KIND), clang++)
WARNINGS += \
	-Wno-undefined-var-template \
	-Wno-absolute-value
GLUE_WARNINGS += \
	-Wno-logical-op-parentheses
BACK_WARNINGS += \
	-Wno-logical-op-parentheses \
	-Wno-constant-conversion
endif
endif

# Treat warnings as errors
ifdef ENABLE_WERROR
WARNINGS += -Werror
endif

OS_UPCASE := $(shell echo $(OS) | tr '[a-z]' '[A-Z]')

MMD=-MMD -MF $(basename $@).deps

# Default compiler flags for all source files
CXXFLAGS := $(WARNINGS) \
	-fno-exceptions -fno-rtti \
	-DMARS=1 -DTARGET_$(OS_UPCASE)=1 -DDM_TARGET_CPU_$(TARGET_CPU)=1 \
	$(MODEL_FLAG) $(PIC)
# GCC Specific
ifeq ($(CXX_KIND), g++)
CXXFLAGS += \
	-std=c++17
endif
# Clang Specific
ifeq ($(CXX_KIND), clang++)
CXXFLAGS += \
	-xc++ -std=c++17
endif
# Default D compiler flags for all source files
DFLAGS := -version=MARS $(PIC)
# Enable D warnings
DFLAGS += -w -de

ifneq (,$(DEBUG))
ENABLE_DEBUG := 1
endif
ifneq (,$(RELEASE))
ENABLE_RELEASE := 1
endif

# Append different flags for debugging, profiling and release.
ifdef ENABLE_DEBUG
CXXFLAGS += -g -g3 -DDEBUG=1 -DUNITTEST
DFLAGS += -g -debug
endif
ifdef ENABLE_RELEASE
CXXFLAGS += -O2
DFLAGS += -O -release -inline
endif
ifdef ENABLE_PROFILING
CXXFLAGS  += -pg -fprofile-arcs -ftest-coverage
endif
ifdef ENABLE_PGO_GENERATE
CXXFLAGS  += -fprofile-generate=${PGO_DIR}
endif
ifdef ENABLE_PGO_USE
CXXFLAGS  += -fprofile-use=${PGO_DIR} -freorder-blocks-and-partition
endif
ifdef ENABLE_LTO
CXXFLAGS  += -flto
endif
ifdef ENABLE_UNITTEST
DFLAGS  += -unittest -cov
endif
ifdef ENABLE_PROFILE
DFLAGS  += -profile
endif
ifdef ENABLE_COVERAGE
DFLAGS  += -cov -L-lgcov
CXXFLAGS += --coverage
endif
ifdef ENABLE_SANITIZERS
CXXFLAGS += -fsanitize=${ENABLE_SANITIZERS}

ifeq ($(HOST_DMD_KIND), dmd)
HOST_CXX += -fsanitize=${ENABLE_SANITIZERS}
endif
ifneq (,$(findstring gdc,$(HOST_DMD_KIND))$(findstring ldc,$(HOST_DMD_KIND)))
DFLAGS += -fsanitize=${ENABLE_SANITIZERS}
endif

endif

# Unique extra flags if necessary
DMD_FLAGS  := -I$(ROOT) $(DMD_WARNINGS)
GLUE_FLAGS := -I$(ROOT) -I$(TK) -I$(C) $(GLUE_WARNINGS)
BACK_FLAGS := -I$(ROOT) -I$(TK) -I$(C) -I. -DDMDV2=1 $(BACK_WARNINGS)
ROOT_FLAGS := -I$(ROOT) $(ROOT_WARNINGS)

# GCC Specific
ifeq ($(CXX_KIND), g++)
BACK_FLAGS += \
	-std=gnu++17
endif

DMD_OBJS = \
	access.o attrib.o \
	dcast.o \
	dclass.o \
	constfold.o cond.o \
	declaration.o dsymbol.o \
        denum.o expression.o expressionsem.o func.o \
	id.o \
	identifier.o impcnvtab.o dimport.o inifile.o init.o initsem.o inline.o inlinecost.o \
	lexer.o link.o dmangle.o mars.o dmodule.o mtype.o \
	compiler.o cppmangle.o opover.o optimize.o \
	parse.o dscope.o statement.o \
	dstruct.o dtemplate.o \
	dversion.o utf.o staticassert.o staticcond.o \
	entity.o doc.o dmacro.o \
	hdrgen.o delegatize.o dinterpret.o traits.o \
	builtin.o ctfeexpr.o clone.o aliasthis.o \
	arrayop.o json.o unittests.o \
	imphint.o argtypes.o apply.o sapply.o safe.o sideeffect.o \
	intrange.o blockexit.o canthrow.o target.o nspace.o errors.o \
	escape.o tokens.o globals.o \
	utils.o chkformat.o \
	dsymbolsem.o semantic2.o semantic3.o statementsem.o templateparamsem.o typesem.o

ROOT_OBJS = \
	rmem.o port.o stringtable.o response.o \
	aav.o speller.o outbuffer.o rootobject.o \
	filename.o file.o checkedint.o \
	newdelete.o ctfloat.o

GLUE_OBJS = \
	glue.o msc.o s2ir.o todt.o e2ir.o tocsym.o \
	toobj.o toctype.o toelfdebug.o toir.o \
        irstate.o typinf.o iasm.o iasmdmd.o objc_glue_stubs.o libelf.o scanelf.o

BACK_OBJS = go.o gdag.o gother.o gflow.o gloop.o var.o el.o \
	glocal.o os.o evalu8.o cgcs.o \
	rtlsym.o cgelem.o cgen.o cgreg.o out.o \
	blockopt.o cg.o type.o dt.o \
	debug.o code.o ee.o symbol.o \
	cgcod.o cod5.o outbuf.o \
	bcomplex.o aa.o ti_achar.o \
	ti_pvoid.o pdata.o backconfig.o \
	divcoeff.o dwarf.o dwarfeh.o \
	ph2.o util2.o eh.o tk.o strtold.o \
	$(TARGET_OBJS) elfobj.o

SRC = posix.mak osmodel.mak \
	mars.cpp denum.cpp dstruct.cpp dsymbol.cpp dimport.cpp idgen.cpp impcnvgen.cpp \
	identifier.cpp mtype.cpp expression.cpp expressionsem.cpp optimize.cpp template.hpp \
	dtemplate.cpp lexer.cpp declaration.cpp dcast.cpp cond.hpp cond.cpp link.cpp \
	aggregate.hpp parse.cpp statement.cpp constfold.cpp version.hpp dversion.cpp \
	inifile.cpp dmodule.cpp dscope.cpp init.hpp init.cpp initsem.cpp attrib.hpp \
        attrib.cpp opover.cpp dclass.cpp dmangle.cpp func.cpp inline.cpp inlinecost.cpp \
	access.cpp complex_t.hpp \
	identifier.hpp parse.hpp \
	scope.hpp enum.hpp import.hpp mars.hpp module.hpp mtype.hpp dsymbol.hpp \
	declaration.hpp lexer.hpp expression.hpp statement.hpp \
	utf.hpp utf.cpp staticassert.hpp staticassert.cpp staticcond.cpp \
	entity.cpp \
	doc.hpp doc.cpp macro.hpp dmacro.cpp hdrgen.hpp hdrgen.cpp arraytypes.hpp \
	delegatize.cpp dinterpret.cpp traits.cpp cppmangle.cpp \
	builtin.cpp clone.cpp lib.hpp arrayop.cpp \
	aliasthis.hpp aliasthis.cpp json.hpp json.cpp unittests.cpp imphint.cpp \
	argtypes.cpp apply.cpp sapply.cpp safe.cpp sideeffect.cpp \
	intrange.hpp intrange.cpp blockexit.cpp canthrow.cpp target.cpp target.hpp \
	ctfe.hpp ctfeexpr.cpp \
	ctfe.hpp ctfeexpr.cpp visitor.hpp nspace.hpp nspace.cpp errors.hpp errors.cpp \
	escape.cpp tokens.hpp tokens.cpp globals.hpp globals.cpp \
	utils.cpp chkformat.cpp \
	dsymbolsem.cpp semantic2.cpp semantic3.cpp statementsem.cpp templateparamsem.cpp typesem.cpp

ROOT_SRC = $(ROOT)/root.hpp \
	$(ROOT)/array.hpp \
	$(ROOT)/rmem.hpp $(ROOT)/rmem.cpp $(ROOT)/port.hpp $(ROOT)/port.cpp \
	$(ROOT)/newdelete.cpp \
	$(ROOT)/checkedint.hpp $(ROOT)/checkedint.cpp \
	$(ROOT)/stringtable.hpp $(ROOT)/stringtable.cpp \
	$(ROOT)/response.cpp \
	$(ROOT)/aav.hpp $(ROOT)/aav.cpp \
	$(ROOT)/longdouble.hpp \
	$(ROOT)/speller.hpp $(ROOT)/speller.cpp \
	$(ROOT)/outbuffer.hpp $(ROOT)/outbuffer.cpp \
	$(ROOT)/object.hpp $(ROOT)/rootobject.cpp \
	$(ROOT)/filename.hpp $(ROOT)/filename.cpp \
	$(ROOT)/file.hpp $(ROOT)/file.cpp \
	$(ROOT)/ctfloat.hpp $(ROOT)/ctfloat.cpp \
	$(ROOT)/hash.hpp

GLUE_SRC = glue.cpp msc.cpp s2ir.cpp todt.cpp e2ir.cpp tocsym.cpp \
	toobj.cpp toctype.cpp tocvdebug.cpp toir.hpp toir.cpp \
	irstate.hpp irstate.cpp typinf.cpp iasm.cpp \
	toelfdebug.cpp libelf.cpp scanelf.cpp \
	tk.cpp eh.cpp gluestub.cpp objc_glue.cpp objc_glue_stubs.cpp

BACK_SRC = \
	$C/cdef.hpp $C/cc.hpp $C/oper.hpp $C/ty.hpp $C/optabgen.cpp \
	$C/global.hpp $C/code.hpp $C/type.hpp $C/dt.hpp \
	$C/el.hpp $C/iasm.hpp $C/rtlsym.hpp \
	$C/bcomplex.cpp $C/blockopt.cpp $C/cg.cpp $C/cg87.cpp $C/cgxmm.cpp \
	$C/cgcod.cpp $C/cgcs.cpp $C/cgelem.cpp $C/cgen.cpp $C/cgobj.cpp \
	$C/cgreg.cpp $C/var.cpp $C/strtold.cpp \
	$C/cgsched.cpp $C/cod1.cpp $C/cod2.cpp $C/cod3.cpp $C/cod4.cpp $C/cod5.cpp \
	$C/code.cpp $C/symbol.cpp $C/debug.cpp $C/dt.cpp $C/ee.cpp $C/el.cpp \
	$C/evalu8.cpp $C/go.cpp $C/gflow.cpp $C/gdag.cpp \
	$C/gother.cpp $C/glocal.cpp $C/gloop.cpp $C/newman.cpp \
	$C/os.cpp $C/out.cpp $C/outbuf.cpp $C/ptrntab.cpp $C/rtlsym.cpp \
	$C/type.cpp $C/melf.hpp  $C/bcomplex.hpp \
	$C/outbuf.hpp $C/token.hpp $C/tassert.hpp \
	$C/elfobj.cpp $C/dwarf2.hpp $C/exh.hpp $C/go.hpp \
	$C/dwarf.cpp $C/dwarf.hpp $C/aa.hpp $C/aa.cpp $C/tinfo.hpp $C/ti_achar.cpp \
	$C/ti_pvoid.cpp $C/platform_stub.cpp $C/code_x86.hpp $C/code_stub.hpp \
	$C/mscoffobj.cpp \
	$C/xmm.hpp $C/obj.hpp $C/pdata.cpp $C/backconfig.cpp $C/divcoeff.cpp \
	$C/md5.cpp $C/md5.hpp \
	$C/ph2.cpp $C/util2.cpp $C/dwarfeh.cpp \
	$(TARGET_CH)

TK_SRC = \
        $(TK)/mem.hpp $(TK)/list.hpp $(TK)/vec.hpp \
        $(TK)/mem.cpp $(TK)/vec.cpp $(TK)/list.cpp

DEPS = $(patsubst %.o,%.deps,$(DMD_OBJS) $(ROOT_OBJS) $(GLUE_OBJS) $(BACK_OBJS))

all: dmd

auto-tester-build: dmd
.PHONY: auto-tester-build

frontend.a: $(DMD_OBJS)
	$(AR) rcs frontend.a $(DMD_OBJS)

root.a: $(ROOT_OBJS)
	$(AR) rcs root.a $(ROOT_OBJS)

glue.a: $(GLUE_OBJS)
	$(AR) rcs glue.a $(GLUE_OBJS)

backend.a: $(BACK_OBJS)
	$(AR) rcs backend.a $(BACK_OBJS)

ifdef ENABLE_LTO
dmd: $(DMD_OBJS) $(ROOT_OBJS) $(GLUE_OBJS) $(BACK_OBJS)
	$(CXX) -o dmd $(MODEL_FLAG) $^ $(LDFLAGS)
	cp dmd $G/dmd
else
dmd: frontend.a root.a glue.a backend.a
	$(CXX) -o dmd $(MODEL_FLAG) frontend.a root.a glue.a backend.a $(LDFLAGS)
	cp dmd $G/dmd
endif

clean:
	rm -f $(DMD_OBJS) $(ROOT_OBJS) $(GLUE_OBJS) $(BACK_OBJS) dmd optab.o id.o impcnvgen idgen id.cpp id.hpp \
		impcnvtab.d id.d impcnvtab.cpp optabgen debtab.cpp optab.cpp cdxxx.cpp elxxx.cpp fltables.cpp \
		tytab.cpp verstr.hpp core \
		*.cov *.deps *.gcda *.gcno *.a \
		$(GENSRC)
	@[ ! -d ${PGO_DIR} ] || echo You should issue manually: rm -rf ${PGO_DIR}
	rm -Rf $(GENERATED)

######## generate a default dmd.conf

define DEFAULT_DMD_CONF
[Environment32]
DFLAGS=-I%@P%/../../druntime/import -I%@P%/../../phobos -L-L%@P%/../../phobos/generated/$(OS)/release/32 -L--export-dynamic

[Environment64]
DFLAGS=-I%@P%/../../druntime/import -I%@P%/../../phobos -L-L%@P%/../../phobos/generated/$(OS)/release/64 -L--export-dynamic -fPIC
endef

export DEFAULT_DMD_CONF

dmd.conf:
	[ -f $@ ] || echo "$$DEFAULT_DMD_CONF" > $@

######## optabgen generates some source

optabgen: $C/optabgen.cpp $C/cc.hpp $C/oper.hpp
	$(HOST_CXX) $(CXXFLAGS) $(BACK_WARNINGS) -I$(TK) $< -o optabgen
	./optabgen

optabgen_output = debtab.cpp optab.cpp cdxxx.cpp elxxx.cpp fltables.cpp tytab.cpp
$(optabgen_output) : optabgen

######## idgen generates some source

idgen_output = id.hpp id.cpp id.d
$(idgen_output) : idgen

idgen : idgen.cpp
	$(HOST_CXX) $(CXXFLAGS) idgen.cpp -o idgen
	./idgen

######### impcnvgen generates some source

impcnvtab_output = impcnvtab.cpp impcnvtab.d
$(impcnvtab_output) : impcnvgen

impcnvgen : mtype.hpp impcnvgen.cpp
	$(HOST_CXX) $(CXXFLAGS) -I$(ROOT) impcnvgen.cpp -o impcnvgen
	./impcnvgen

#########

# Create (or update) the verstr.hpp file.
# The file is only updated if the VERSION file changes, or, only when RELEASE=1
# is not used, when the full version string changes (i.e. when the git hash or
# the working tree dirty states changes).
# The full version string have the form VERSION-devel-HASH(-dirty).
# The "-dirty" part is only present when the repository had uncommitted changes
# at the moment it was compiled (only files already tracked by git are taken
# into account, untracked files don't affect the dirty state).
VERSION := 0.0.1
ifneq (1,$(RELEASE))
VERSION_GIT := $(shell printf "`$(GIT) rev-parse --short HEAD`"; \
       test -n "`$(GIT) status --porcelain -uno`" && printf -- -dirty)
VERSION := $(addsuffix -devel$(if $(VERSION_GIT),-$(VERSION_GIT)),$(VERSION))
endif
$(shell test \"$(VERSION)\" != "`cat verstr.hpp 2> /dev/null`" \
		&& printf \"$(VERSION)\" > verstr.hpp )

#########

$(DMD_OBJS) $(GLUE_OBJS) : $(idgen_output) $(impcnvgen_output)
$(BACK_OBJS) : $(optabgen_output)


# Specific dependencies other than the source file for all objects
########################################################################
# If additional flags are needed for a specific file add a _CFLAGS as a
# dependency to the object file and assign the appropriate content.

cg.o: fltables.cpp

cgcod.o: cdxxx.cpp

cgelem.o: elxxx.cpp

debug.o: debtab.cpp

iasm.o: CXXFLAGS += -fexceptions

inifile.o: CXXFLAGS += -DSYSCONFDIR='"$(SYSCONFDIR)"'

mars.o: verstr.hpp

var.o: optab.cpp tytab.cpp


# Generic rules for all source files
########################################################################
# Search the directory $(C) for .c-files when using implicit pattern
# matching below.
vpath %.cpp $(C)

$(DMD_OBJS): %.o: %.cpp posix.mak
	@echo "  (CC)  DMD_OBJS   $<"
	$(CXX) -c $(CXXFLAGS) $(DMD_FLAGS) $(MMD) $<

$(BACK_OBJS): %.o: %.cpp posix.mak
	@echo "  (CC)  BACK_OBJS  $<"
	$(CXX) -c $(CXXFLAGS) $(BACK_FLAGS) $(MMD) $<

$(GLUE_OBJS): %.o: %.cpp posix.mak
	@echo "  (CC)  GLUE_OBJS  $<"
	$(CXX) -c $(CXXFLAGS) $(GLUE_FLAGS) $(MMD) $<

$(ROOT_OBJS): %.o: $(ROOT)/%.cpp posix.mak
	@echo "  (CC)  ROOT_OBJS  $<"
	$(CXX) -c $(CXXFLAGS) $(ROOT_FLAGS) $(MMD) $<


-include $(DEPS)

######################################################

install: all
	$(eval bin_dir=$(if $(filter $(OS),osx), bin, bin$(MODEL)))
	mkdir -p $(INSTALL_DIR)/$(OS)/$(bin_dir)
	cp dmd $(INSTALL_DIR)/$(OS)/$(bin_dir)/dmd
	cp ../ini/$(OS)/$(bin_dir)/dmd.conf $(INSTALL_DIR)/$(OS)/$(bin_dir)/dmd.conf
	cp backendlicense.txt $(INSTALL_DIR)/dmd-backendlicense.txt
	cp boostlicense.txt $(INSTALL_DIR)/dmd-boostlicense.txt

######################################################

gcov:
	gcov access.cpp
	gcov aliasthis.cpp
	gcov apply.cpp
	gcov arrayop.cpp
	gcov attrib.cpp
	gcov builtin.cpp
	gcov blockexit.cpp
	gcov canthrow.cpp
	gcov dcast.cpp
	gcov dclass.cpp
	gcov clone.cpp
	gcov cond.cpp
	gcov constfold.cpp
	gcov declaration.cpp
	gcov delegatize.cpp
	gcov doc.cpp
	gcov dsymbol.cpp
	gcov e2ir.cpp
	gcov eh.cpp
	gcov entity.cpp
	gcov denum.cpp
	gcov expression.cpp
	gcov expressionsem.cpp
	gcov func.cpp
	gcov glue.cpp
	gcov iasm.cpp
	gcov identifier.cpp
	gcov imphint.cpp
	gcov dimport.cpp
	gcov inifile.cpp
	gcov init.cpp
	gcov initsem.cpp
	gcov inline.cpp
	gcov inlinecost.cpp
	gcov dinterpret.cpp
	gcov ctfeexpr.cpp
	gcov irstate.cpp
	gcov json.cpp
	gcov lexer.cpp
	gcov libelf.cpp
	gcov link.cpp
	gcov dmacro.cpp
	gcov dmangle.cpp
	gcov mars.cpp
	gcov dmodule.cpp
	gcov msc.cpp
	gcov mtype.cpp
	gcov nspace.cpp
	gcov objc_glue_stubs.cpp
	gcov opover.cpp
	gcov optimize.cpp
	gcov parse.cpp
	gcov dscope.cpp
	gcov safe.cpp
	gcov sideeffect.cpp
	gcov statement.cpp
	gcov staticassert.cpp
	gcov staticcond.cpp
	gcov s2ir.cpp
	gcov dstruct.cpp
	gcov dtemplate.cpp
	gcov tk.cpp
	gcov tocsym.cpp
	gcov todt.cpp
	gcov toobj.cpp
	gcov toctype.cpp
	gcov toelfdebug.cpp
	gcov typinf.cpp
	gcov utf.cpp
	gcov dversion.cpp
	gcov intrange.cpp
	gcov target.cpp

#	gcov hdrgen.cpp

######################################################

zip:
	-rm -f dmdsrc.zip
	zip dmdsrc $(SRC) $(ROOT_SRC) $(GLUE_SRC) $(BACK_SRC) $(TK_SRC)

#############################

.DELETE_ON_ERROR: # GNU Make directive (delete output files on error)
