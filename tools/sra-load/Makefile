# ==============================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

default: std

TOP ?= $(abspath ../..)
MODULE = tools/sra-load

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = \
	fastq-load \
	illumina-load \
	srf-load \
	abi-load \
	sff-load \
	helicos-load \
	pacbio-loadxml

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (win,$(OS))
all std:
	@ echo "not building loaders under Windows"
else
all std: vers-includes
	@ $(MAKE_CMD) $(TARGDIR)/std
endif

$(ALL_TOOLS): vers-includes
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# vers-includes
#
$(TARGDIR)/vers-includes: $(addsuffix .vers.h,$(EXT_TOOLS))

.PHONY: $(TARGDIR)/vers-includes

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean
	@ rm -f *.vers.h

.PHONY: clean

#-------------------------------------------------------------------------------
# loader
#
LOADER_SRC = \
	pstring \
	common-xml \
	experiment-xml \
	run-xml \
	loader-file \
	loader-fmt \
	sra-writer \
	loader

LOADER_OBJ = \
	$(addsuffix .$(OBJX),$(LOADER_SRC))

LIBXML = -lkxfs -lkxml -lxml2
    
LOADER_LIB = \
	-lkapp \
	-lload \
	-sncbi-wvdb \
	$(LIBXML) \
	-lm


#------------------------------------------------------------------------------
# sff-load
#
SFF_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-454 \
	writer-ion-torrent \
	sff-fmt

SFF_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(SFF_LOAD_SRC))

$(BINDIR)/sff-load: $(SFF_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR) -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# fastq-load
#
FASTQ_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-illumina \
	writer-454 \
	writer-ion-torrent \
	fastq-fmt

FASTQ_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(FASTQ_LOAD_SRC))

$(BINDIR)/fastq-load: $(FASTQ_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR) -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# srf-load
#
SRF_LOAD_SRC = \
	$(LOADER_SRC) \
	srf \
	srf-fmt \
	writer-illumina \
	ztr-illumina \
	srf-illumina \
	writer-absolid \
	ztr-absolid \
	ztr-huffman \
	srf-absolid

SRF_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(SRF_LOAD_SRC))

$(BINDIR)/srf-load: $(SRF_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR) -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# abi-load
#
ABSOLID_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-absolid \
	absolid-fmt

ABSOLID_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(ABSOLID_LOAD_SRC))

$(BINDIR)/abi-load: $(ABSOLID_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR) -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# illumina-load
#
ILLUMINA_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-illumina \
	illumina-fmt

ILLUMINA_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(ILLUMINA_LOAD_SRC))

$(BINDIR)/illumina-load: $(ILLUMINA_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR) -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# helicos-load
#
HELICOS_LOAD_SRC = \
	$(LOADER_SRC) \
	writer-helicos \
	helicos-fmt

HELICOS_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(HELICOS_LOAD_SRC))

$(BINDIR)/helicos-load: $(HELICOS_LOAD_OBJ)
	$(LD) --exe --vers $(SRCDIR) -o $@ $^ $(LOADER_LIB)

#------------------------------------------------------------------------------
# pacbio-loadxml
#
PACBIO_LOADXML_SRC = \
	$(LOADER_SRC) \
	pacbio-fmt

PACBIO_LOADXML_OBJ = \
	$(addsuffix .$(OBJX),$(PACBIO_LOADXML_SRC))

$(BINDIR)/pacbio-loadxml: $(PACBIO_LOADXML_OBJ)
	$(LD) --exe --vers $(SRCDIR) -o $@ $^ $(LOADER_LIB)
