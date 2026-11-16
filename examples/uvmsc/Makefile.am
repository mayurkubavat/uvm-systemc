include $(top_srcdir)/config/Make-rules.examples

all:
	@echo "To compile and run the examples call"
	@echo "   make check "


# begin with empty variables, is filled later in test.am files
examples_BUILD = 
examples_CLEAN = 
examples_DIRS =
examples_FILES = 
examples_TESTS = 
examples_XFAIL_TESTS = 


# include simple tests
include $(srcdir)/simple/callbacks/basic/test.am
include $(srcdir)/simple/configuration/manual/test.am
include $(srcdir)/simple/factory/basic/test.am
include $(srcdir)/simple/hello_world/test.am
include $(srcdir)/simple/objections/basic/test.am
include $(srcdir)/simple/phases/basic/test.am
include $(srcdir)/simple/phases/timeout/test.am
include $(srcdir)/simple/registers/models/aliasing/test.am
include $(srcdir)/simple/registers/models/ro_wo_same_addr/test.am
include $(srcdir)/simple/registers/models/fifo_reg/test.am
include $(srcdir)/simple/registers/models/reg_without_field/test.am
include $(srcdir)/simple/sequence/basic_read_write_sequence/test.am
include $(srcdir)/simple/tlm1/bidir/test.am
include $(srcdir)/simple/tlm1/hierarchy/test.am
include $(srcdir)/simple/tlm1/producer_consumer/test.am
include $(srcdir)/simple/trivial/test.am
include $(srcdir)/integrated/ubus/test.am

# The CRAVE examples need an installation of CRAVE and are only built with
# CMake (UVM_SYSTEMC_ENABLE_CRAVE), so they are distributed without a test.
examples_DIRS += \
				 simple/crave/constrained_random \
				 simple/crave/sequence_randomization \
				 simple/crave

examples_FILES += \
	simple/crave/constrained_random/packet.h \
	simple/crave/constrained_random/packet_driver.h \
	simple/crave/constrained_random/packet_sequence.h \
	simple/crave/constrained_random/packet_sequencer.h \
	simple/crave/constrained_random/sc_main.cpp \
	simple/crave/constrained_random/testbench.h \
	simple/crave/sequence_randomization/sc_main.cpp

examples_DIRS += \
				 simple/callbacks \
				 simple/configuration \
				 simple/factory \
				 simple/objections \
				 simple/phases \
				 simple/registers/models \
				 simple/registers \
				 simple/sequence \
				 simple/tlm1 \
				 simple \
				 integrated

CLEANFILES += $(examples_CLEAN)

check_PROGRAMS = $(examples_TESTS)

TESTS = $(check_PROGRAMS)

XFAIL_TESTS = $(examples_XFAIL_TESTS)

# install examples?
if SEPARATE_INSTALL_TREE
nobase_examples_DATA = \
	$(examples_FILES)

uninstall-hook:
	test ! -d "$(examplesdir)" || ( set -e ; cd "$(examplesdir)" ; \
		for dir in $(examples_DIRS) ; do test ! -d $$dir || rmdir $$dir ; done ; \
		cd - ; rmdir "$(examplesdir)" )

endif # SEPARATE_INSTALL_TREE

EXTRA_DIST += \
	$(examples_FILES)

CLEANFILES += \
	$(examples_CLEAN) \
	$(check_SCRIPTS)

## Taf!
