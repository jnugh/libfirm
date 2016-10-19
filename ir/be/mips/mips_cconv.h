/*
 * This file is part of libFirm.
 * Copyright (C) 2017 University of Karlsruhe.
 */

#ifndef FIRM_BE_MIPS_MIPS_CCONV_H
#define FIRM_BE_MIPS_MIPS_CCONV_H

#include "be_types.h"

typedef struct mips_reg_or_slot_t {
	arch_register_t const *reg;
} mips_reg_or_slot_t;

typedef struct mips_calling_convention_t {
	mips_reg_or_slot_t *results;
} mips_calling_convention_t;

void mips_determine_calling_convention(mips_calling_convention_t *cconv, ir_type *fun_type);

void mips_free_calling_convention(mips_calling_convention_t *cconv);

#endif
