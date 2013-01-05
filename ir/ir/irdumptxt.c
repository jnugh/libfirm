/*
 * This file is part of libFirm.
 * Copyright (C) 2012 University of Karlsruhe.
 */

/**
 * @file
 * @brief   Write text representation of firm to file.
 * @author  Martin Trapp, Christian Schaefer, Goetz Lindenmaier, Hubert Schmidt,
 *          Matthias Braun
 */
#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "irdump_t.h"
#include "irgraph_t.h"
#include "irnode_t.h"

#include "irprog_t.h"
#include "entity_t.h"
#include "trouts.h"
#include "irgwalk.h"
#include "tv_t.h"
#include "irprintf.h"
#include "error.h"

#include "irdom.h"

static ir_dump_verbosity_t  verbosity = dump_verbosity_max;

void ir_set_dump_verbosity(ir_dump_verbosity_t new_verbosity)
{
	verbosity = new_verbosity;
}

ir_dump_verbosity_t ir_get_dump_verbosity(void)
{
	return verbosity;
}

void dump_irnode_to_file(FILE *F, const ir_node *n)
{
	char     comma;
	ir_graph *irg;

	dump_node_opcode(F, n);
	fprintf(F, " %ld\n", get_irn_node_nr(n));

	fprintf(F, "  index: %u\n", get_irn_idx(n));
	if (ir_get_dump_flags() & ir_dump_flag_analysed_types)
		fprintf (F, "  addr:    %p\n", (void *)n);
	fprintf (F, "  mode:    %s\n", get_mode_name(get_irn_mode(n)));
	fprintf (F, "  visited: %lu\n", get_irn_visited(n));
	irg = get_irn_irg(n);
	if (irg != get_const_code_irg())
		fprintf (F, "  irg:     %s\n", get_ent_dump_name(get_irg_entity(irg)));

	if (get_irn_pinned(n) == op_pin_state_floats &&
		get_irg_pinned(get_irn_irg(n)) == op_pin_state_floats) {
		fprintf(F, "  node was pinned in ");
		ir_node *const block = get_nodes_block(n);
		dump_node_opcode(F, block);
		fprintf(F, " %ld\n", get_irn_node_nr(block));
	}

	fprintf(F, "  arity:   %d\n", get_irn_arity(n));
	/* show all predecessor nodes */
	fprintf(F, "  pred nodes:\n");
	if (!is_Block(n)) {
		fprintf(F, "    -1:    ");
		ir_node *const block = get_nodes_block(n);
		dump_node_opcode(F, block);
		fprintf(F, " %ld\n", get_irn_node_nr(block));
	}

	{
		int i;
		for (i = 0; i < get_irn_arity(n); ++i) {
			fprintf(F, "     %d: %s ", i, is_backedge(n, i) ? "be" : "  ");
			dump_node_opcode(F, get_irn_n(n, i));
			fprintf(F, " %ld\n", get_irn_node_nr(get_irn_n(n, i)));
		}
	}

	fprintf(F, "  Private Attributes:\n");

	if (is_Proj(n)) {
		ir_node *pred = get_Proj_pred(n);
		long     pn   = get_Proj_proj(n);
		fprintf(F, "  proj nr: %ld\n", pn);
		if (is_Switch(pred)) {
			const ir_switch_table *table = get_Switch_table(pred);
			size_t n_entries = ir_switch_table_get_n_entries(table);
			size_t i;
			for (i = 0; i < n_entries; ++i) {
				const ir_switch_table_entry *entry
					= ir_switch_table_get_entry_const(table, i);
				if (entry->pn == pn && entry->min != NULL && entry->max != NULL) {
					ir_tarval *min = entry->min;
					ir_tarval *max = entry->max;
					if (min != max) {
						ir_fprintf(F, "  switch case %+F .. %+F\n", min, max);
					} else {
						ir_fprintf(F, "  switch case %+F\n", min);
					}
				}
			}
		}
	}

	if (is_fragile_op(n)) {
		fprintf(F, "  pinned state: %s\n", get_op_pin_state_name(get_irn_pinned(n)));
		/* not dumped: frag array */
	}

	/* This is not nice, output it as a marker in the predecessor list. */
	if (is_Block(n) || is_Phi(n)) {
	    int i;
		fprintf(F, "  backedges:");
		comma = ' ';
		for (i = 0; i < get_irn_arity(n); i++)
			if (is_backedge(n, i)) { fprintf(F, "%c %d", comma, i); comma = ','; }
			fprintf(F, "\n");
	}

	/* Loop node.   Someone else please tell me what's wrong ... */
	if (irg_has_properties(irg, IR_GRAPH_PROPERTY_CONSISTENT_LOOPINFO)) {
		ir_loop *loop = get_irn_loop(n);
		if (loop != NULL) {
			fprintf(F, "  in loop %ld with depth %u\n",
			        get_loop_loop_nr(loop), get_loop_depth(loop));
		}
	}

	/* Source types */
	switch (get_irn_opcode(n)) {
	case iro_Block: {
		ir_entity *const entity = get_Block_entity(n);
		if (entity != NULL)
			fprintf(F, "  Label: %lu\n", get_entity_label(entity));
		fprintf(F, "  block visited: %lu\n", get_Block_block_visited(n));
		fprintf(F, "  block marked: %u\n", get_Block_mark(n));
		if (irg_has_properties(get_irn_irg(n), IR_GRAPH_PROPERTY_CONSISTENT_DOMINANCE)) {
			fprintf(F, "  dom depth %d\n", get_Block_dom_depth(n));
			fprintf(F, "  domtree pre num %u\n", get_Block_dom_tree_pre_num(n));
			fprintf(F, "  max subtree pre num %u\n", get_Block_dom_max_subtree_pre_num(n));
		}
		if (irg_has_properties(get_irn_irg(n), IR_GRAPH_PROPERTY_CONSISTENT_POSTDOMINANCE)) {
			fprintf(F, "  pdom depth %d\n", get_Block_postdom_depth(n));
			fprintf(F, "  pdomtree pre num %u\n", get_Block_pdom_tree_pre_num(n));
			fprintf(F, "  max pdomsubtree pre num %u\n", get_Block_pdom_max_subtree_pre_num(n));
		}

		/* not dumped: graph_arr */
		/* not dumped: mature    */
	}  break;
	case iro_Start: {
		size_t   i;
		ir_type *tp = get_entity_type(get_irg_entity(get_irn_irg(n)));
		ir_fprintf(F, "  start of method of type %+F\n", tp);
		for (i = 0; i < get_method_n_params(tp); ++i)
			ir_fprintf(F, "    param %d type: %+F\n", i, get_method_param_type(tp, i));
	} break;
	case iro_Cond: {
		if (get_Cond_jmp_pred(n) != COND_JMP_PRED_NONE) {
			fprintf(F, "  jump prediction: %s\n",
			        get_cond_jmp_predicate_name(get_Cond_jmp_pred(n)));
		}
	} break;
	case iro_Alloc: {
		ir_fprintf(F, "  allocating entity of type: %+F\n", get_Alloc_type(n));
		fprintf(F, "  allocating on: the %s\n", (get_Alloc_where(n) == stack_alloc) ? "stack" : "heap");
	} break;
	case iro_Free: {
		ir_fprintf(F, "  freeing entity of type %+F\n", get_Free_type(n));
		fprintf(F, "  allocated on: the %s\n", (get_Free_where(n) == stack_alloc) ? "stack" : "heap");
	} break;
	case iro_Sel: {
		ir_entity *ent = get_Sel_entity(n);
		if (ent) {
			fprintf(F, "  Selecting entity %s (%ld)\n", get_entity_name(ent), get_entity_nr(ent));
			ir_fprintf(F, "    of type    %+F\n",  get_entity_type(ent));
			ir_fprintf(F, "    with owner %+F.\n", get_entity_owner(ent));
		} else {
			fprintf(F, "  <NULL entity>\n");
		}
	} break;
	case iro_Call: {
		ir_type *tp = get_Call_type(n);
		ir_fprintf(F, "  calling method of type %+F\n", tp);
		if (get_unknown_type() != tp) {
			size_t i;
			for (i = 0; i < get_method_n_params(tp); ++i)
				ir_fprintf(F, "    param %d type: %+F\n", i, get_method_param_type(tp, i));
			for (i = 0; i < get_method_n_ress(tp); ++i)
				ir_fprintf(F, "    result %d type: %+F\n", i, get_method_res_type(tp, i));
		}
		if (Call_has_callees(n)) {
			size_t i;
			fprintf(F, "  possible callees:\n");
			for (i = 0; i < get_Call_n_callees(n); i++) {
				ir_fprintf(F, "    %zu: %s\n", i, get_ent_dump_name(get_Call_callee(n, i)));
			}
		}
	} break;
	case iro_Cmp: {
		ir_relation relation = get_Cmp_relation(n);
		ir_fprintf(F, "  relation: %s\n", get_relation_string(relation));
	} break;
	case iro_Return: {
		size_t   i;
		ir_type *tp = get_entity_type(get_irg_entity(get_irn_irg(n)));
		ir_fprintf(F, "  return in method of type %+F\n", tp);
		for (i = 0; i < get_method_n_ress(tp); ++i) {
			ir_fprintf(F, "    result %d type: %+F\n", i,
					   get_method_res_type(tp, i));
		}
	} break;
	case iro_SymConst: {
		switch (get_SymConst_kind(n)) {
		case symconst_addr_ent:
			fprintf(F, "  kind:   addr_ent\n");
			fprintf(F, "  entity: ");
			dump_entity_to_file(F, get_SymConst_entity(n));
			break;
		case symconst_ofs_ent:
			fprintf(F, "  kind:   offset\n");
			fprintf(F, "  entity: ");
			dump_entity_to_file(F, get_SymConst_entity(n));
			break;
		case symconst_type_size:
			fprintf(F, "  kind: size\n");
			fprintf(F, "  type: ");
			dump_type_to_file(F, get_SymConst_type(n));
			break;
		case symconst_type_align:
			fprintf(F, "  kind: alignment\n");
			fprintf(F, "  type: ");
			dump_type_to_file(F, get_SymConst_type(n));
			break;
		case symconst_enum_const:
			fprintf(F, "  kind: enumeration\n");
			fprintf(F, "  name: %s\n", get_enumeration_const_name(get_SymConst_enum(n)));
			break;
		}
	} break;
	case iro_Load:
		fprintf(F, "  mode of loaded value: %s\n", get_mode_name_ex(get_Load_mode(n)));
		fprintf(F, "  volatility: %s\n", get_volatility_name(get_Load_volatility(n)));
		fprintf(F, "  align: %s\n", get_align_name(get_Load_unaligned(n)));
		break;
	case iro_Store:
		fprintf(F, "  volatility: %s\n", get_volatility_name(get_Store_volatility(n)));
		fprintf(F, "  align: %s\n", get_align_name(get_Store_unaligned(n)));
		break;
	case iro_Confirm:
		fprintf(F, "  compare operation: %s\n", get_relation_string(get_Confirm_relation(n)));
		break;
	case iro_ASM: {
		fprintf(F, "  assembler text: %s", get_id_str(get_ASM_text(n)));
		fprintf(F, "\n  inputs:  ");
		const ir_asm_constraint *in_cons = get_ASM_input_constraints(n);
		int n_inputs = get_ASM_n_inputs(n);
		for (int i = 0; i < n_inputs; ++i) {
			fprintf(F, "%%%u %s ", in_cons[i].pos,
			        get_id_str(in_cons[i].constraint));
		}
		fprintf(F, "\n  outputs: ");
		const ir_asm_constraint *out_cons = get_ASM_output_constraints(n);
		int n_outputs = get_ASM_n_output_constraints(n);
		for (int i = 0; i < n_outputs; ++i) {
			fprintf(F, "%%%u %s ", out_cons[i].pos,
			        get_id_str(out_cons[i].constraint));
		}

		fprintf(F, "\n  clobber: ");
		ident **clobber = get_ASM_clobbers(n);
		int n_clobbers = get_ASM_n_clobbers(n);
		for (int i = 0; i < n_clobbers; ++i)
			fprintf(F, "%s ", get_id_str(clobber[i]));
		if (get_irn_pinned(n) != op_pin_state_floats)
			fprintf(F, "\n  volatile");
		fprintf(F, "\n");
		break;
	}

	default:
		break;
	}

	if (get_irg_typeinfo_state(get_irn_irg(n)) == ir_typeinfo_consistent  ||
		get_irg_typeinfo_state(get_irn_irg(n)) == ir_typeinfo_inconsistent  )
		if (get_irn_typeinfo_type(n) != get_none_type())
			ir_fprintf (F, "  Analysed type: %s\n", get_irn_typeinfo_type(n));
}

void dump_graph_as_text(FILE *out, ir_graph *irg)
{
	fprintf(out, "graph %s\n", get_irg_dump_name(irg));
}

static int need_nl = 1;

static bool is_init_string(ir_initializer_t const* const init, ir_type *const type)
{
	ir_type *const element_type = get_array_element_type(type);
	ir_mode *      mode;
	size_t         n;
	size_t         i;

	if (!is_Primitive_type(element_type))
		return false;

	mode = get_type_mode(element_type);
	if (!mode_is_int(mode) || get_mode_size_bits(mode) != 8)
		return false;

	n = get_initializer_compound_n_entries(init);
	for (i = 0; i != n; ++i) {
		ir_initializer_t const* const val = get_initializer_compound_value(init, i);
		ir_tarval*                    tv;
		long                          v;

		if (get_initializer_kind(val) != IR_INITIALIZER_TARVAL)
			return false;
		tv = get_initializer_tarval_value(val);

		if (!tarval_is_constant(tv))
			return false;

		v = get_tarval_long(tv);
		if (v != 0 && (v < 0x07 || 0x0D < v) && v != 0x1B && (v < 0x20 || 0x80 <= v) && (v < 0xA0 || 0x100 <= v))
			return false;
	}

	return true;
}

/**
 * Dump initializers.
 */
static void dump_ir_initializers_to_file(FILE *F, const char *prefix,
                                         const ir_initializer_t *initializer,
                                         ir_type *type)
{
	ir_tarval *tv;
	ir_node   *value;

	if (need_nl) {
		fprintf(F, "\n%s    ", prefix);
		need_nl = 0;
	}
	switch (get_initializer_kind(initializer)) {
	case IR_INITIALIZER_NULL:
		fprintf(F, "\t = <NOT_SET>");
		break;
	case IR_INITIALIZER_TARVAL:
		tv = get_initializer_tarval_value(initializer);
		ir_fprintf(F, "\t = <TV>%F", tv);
		break;
	case IR_INITIALIZER_CONST:
		value = get_initializer_const_value(initializer);
		ir_fprintf(F, "\t = %F", value);
		break;
	case IR_INITIALIZER_COMPOUND:
		if (is_Array_type(type)) {
			size_t const n = get_initializer_compound_n_entries(initializer);
			size_t       i;

			if (is_init_string(initializer, type)) {
				fprintf(F, "\t[0...%u] = '", (unsigned)n - 1);
				for (i = 0; i != n; ++i) {
					ir_initializer_t const* const val = get_initializer_compound_value(initializer, i);
					ir_tarval*              const tv  = get_initializer_tarval_value(val);
					long                    const v   = get_tarval_long(tv);

					switch (v) {
						case 0x00: fprintf(F, "\\\\000");  break;
						case 0x07: fprintf(F, "\\\\a");    break;
						case 0x08: fprintf(F, "\\\\b");    break;
						case 0x09: fprintf(F, "\\\\t");    break;
						case 0x0A: fprintf(F, "\\\\n");    break;
						case 0x0B: fprintf(F, "\\\\v");    break;
						case 0x0C: fprintf(F, "\\\\f");    break;
						case 0x0D: fprintf(F, "\\\\r");    break;
						case 0x1B: fprintf(F, "\\\\033");  break;
						case 0x22: fprintf(F, "\\\\\\\""); break;
						case 0x5C: fprintf(F, "\\\\\\\\"); break;
						default:   fprintf(F, "%c", (unsigned char)v); break;
					}
				}
				fprintf(F, "'");
			} else {
				ir_type *const element_type = get_array_element_type(type);

				for (i = 0; i < n; ++i) {
					ir_initializer_t *sub_initializer
						= get_initializer_compound_value(initializer, i);

					if (need_nl) {
						fprintf(F, "\n%s    ", prefix);
						need_nl = 0;
					}
					fprintf(F, "[%d]", (int) i);
					dump_ir_initializers_to_file(F, prefix, sub_initializer, element_type);
				}
			}
		} else {
			size_t i, n;
			assert(is_compound_type(type));
			n = get_compound_n_members(type);
			for (i = 0; i < n; ++i) {
				ir_entity        *member    = get_compound_member(type, i);
				ir_type          *subtype   = get_entity_type(member);
				ir_initializer_t *sub_initializer;

				assert(i < get_initializer_compound_n_entries(initializer));
				sub_initializer
					= get_initializer_compound_value(initializer, i);

				if (need_nl) {
					fprintf(F, "\n%s    ", prefix);
					need_nl = 0;
				}
				ir_fprintf(F, ".%F", member);
				dump_ir_initializers_to_file(F, prefix, sub_initializer, subtype);
			}
		}
		break;
	default:
		panic("invalid ir_initializer kind found");
	}
	need_nl = 1;
}

static void dump_entity_linkage(FILE *F, const ir_entity *entity)
{
	ir_linkage linkage = get_entity_linkage(entity);

	if (linkage == IR_LINKAGE_DEFAULT) {
		fprintf(F, " default");
		return;
	}
	if (linkage & IR_LINKAGE_CONSTANT)
		fprintf(F, " constant");
	if (linkage & IR_LINKAGE_WEAK)
		fprintf(F, " weak");
	if (linkage & IR_LINKAGE_GARBAGE_COLLECT)
		fprintf(F, " garbage_collect");
	if (linkage & IR_LINKAGE_MERGE)
		fprintf(F, " merge");
	if (linkage & IR_LINKAGE_HIDDEN_USER)
		fprintf(F, " hidden_user");
}

static void dump_entity_to_file_prefix(FILE *F, ir_entity *ent, const char *prefix)
{
	ir_type *owner, *type;

	assert(is_entity(ent));
	owner = get_entity_owner(ent);
	type  = get_entity_type(ent);
	if (verbosity & dump_verbosity_onlynames) {
		fprintf(F, "%sentity %s.%s (%ld)\n", prefix, get_compound_name(get_entity_owner(ent)),
			get_entity_name(ent), get_entity_nr(ent));
		return;
	}

	if (verbosity & dump_verbosity_entattrs) {
		fprintf(F, "%sentity %s (%ld)\n", prefix, get_entity_name(ent), get_entity_nr(ent));
		ir_fprintf(F, "%s  type:  %+F\n", prefix, type);
		ir_fprintf(F, "%s  owner: %+F\n", prefix, owner);

		if (is_Class_type(get_entity_owner(ent))) {
			if (get_entity_n_overwrites(ent) > 0) {
				size_t i;
				fprintf(F, "%s  overwrites:\n", prefix);
				for (i = 0; i < get_entity_n_overwrites(ent); ++i) {
					ir_entity *ov = get_entity_overwrites(ent, i);
					ir_fprintf(F, "%s    %d: %s of class %+F\n", prefix, i,
					        get_entity_name(ov), get_entity_owner(ov));
				}
			} else {
				fprintf(F, "%s  Does not overwrite other entities.\n", prefix);
			}
			if (get_entity_n_overwrittenby(ent) > 0) {
				size_t i;
				fprintf(F, "%s  overwritten by:\n", prefix);
				for (i = 0; i < get_entity_n_overwrittenby(ent); ++i) {
					ir_entity *ov = get_entity_overwrittenby(ent, i);
					ir_fprintf(F, "%s    %d: %s of class %+F\n", prefix, i,
					           get_entity_name(ov), get_entity_owner(ov));
				}
			} else {
				fprintf(F, "%s  Is not overwritten by other entities.\n",
				        prefix);
			}

			if (get_irp_inh_transitive_closure_state() != inh_transitive_closure_none) {
				ir_entity *ov;
				fprintf(F, "%s  transitive overwrites:\n", prefix);
				for (ov = get_entity_trans_overwrites_first(ent);
				ov;
				ov = get_entity_trans_overwrites_next(ent)) {
					ir_fprintf(F, "%s    : %s of class %+F\n", prefix,
					           get_entity_name(ov), get_entity_owner(ov));
				}
				fprintf(F, "%s  transitive overwritten by:\n", prefix);
				for (ov = get_entity_trans_overwrittenby_first(ent);
				ov;
				ov = get_entity_trans_overwrittenby_next(ent)) {
					ir_fprintf(F, "%s    : %s of class %+F\n", prefix,
					           get_entity_name(ov), get_entity_owner(ov));
				}
			}
		}

		if (is_Method_type(get_entity_type(ent))) {
			unsigned mask = get_entity_additional_properties(ent);
			unsigned cc   = get_method_calling_convention(get_entity_type(ent));
			ir_graph *irg = get_entity_irg(ent);

			if (irg) {
				fprintf(F, "%s  maximum node index:   %u\n", prefix, get_irg_last_idx(irg));
			}

			fprintf(F, "%s  additional prop: ", prefix);
			if (mask & mtp_property_const)         fputs("const_function, ", F);
			if (mask & mtp_property_pure)          fputs("pure_function, ", F);
			if (mask & mtp_property_noreturn)      fputs("noreturn_function, ", F);
			if (mask & mtp_property_nothrow)       fputs("nothrow_function, ", F);
			if (mask & mtp_property_naked)         fputs("naked_function, ", F);
			if (mask & mtp_property_malloc)        fputs("malloc_function, ", F);
			if (mask & mtp_property_returns_twice) fputs("weak_function, ", F);
			if (mask & mtp_property_intrinsic)     fputs("intrinsic_function, ", F);
			if (mask & mtp_property_runtime)       fputs("runtime_function, ", F);
			if (mask & mtp_property_private)       fputs("private_function, ", F);
			if (mask & mtp_property_has_loop)      fputs("has_loop_function, ", F);
			fputc('\n', F);

			fprintf(F, "%s  calling convention: ", prefix);
			if (cc & cc_reg_param)           fputs("regparam, ", F);
			if (cc & cc_this_call)           fputs("thiscall, ", F);
			if (cc & cc_compound_ret)        fputs("compound_ret, ", F);
			if (cc & cc_frame_on_caller_stk) fputs("frame on caller's stack, ", F);
			cc &= ~(cc_compound_ret|cc_frame_on_caller_stk);
			if (IS_CDECL(cc))
				fputs("cdecl", F);
			else if (IS_STDCALL(cc))
				fputs("stdcall", F);
			else {
				fputs(cc & cc_last_on_top      ? "last param on top, " : "first param on top, ", F);
				fputs(cc & cc_callee_clear_stk ? "callee clear stack" : "caller clear stack", F);
			}
			fprintf(F, "\n%s  vtable number:        %u\n", prefix, get_entity_vtable_number(ent));
		}
	} else {  /* no entattrs */
		ir_fprintf(F, "%s(%3d:%d) %+F: %s", prefix,
			get_entity_offset(ent), get_entity_offset_bits_remainder(ent),
			get_entity_type(ent), get_entity_name(ent));
		if (is_Method_type(get_entity_type(ent))) fputs("(...)", F);

		if (verbosity & dump_verbosity_accessStats) {
			dump_entity_linkage(F, ent);
		}
		fputc('\n', F);
	}

	if (verbosity & dump_verbosity_entconsts) {
		if (ent->initializer != NULL) {
			const ir_initializer_t *initializer = get_entity_initializer(ent);
			fprintf(F, "\n%s  Initializers:", prefix);
			need_nl = 1;
			dump_ir_initializers_to_file(F, prefix, initializer, get_entity_type(ent));
			fputc('\n', F);
		}
	}

	if (verbosity & dump_verbosity_entattrs) {
		fprintf(F, "%s  linkage:", prefix);
		dump_entity_linkage(F, ent);
		fprintf(F, "\n%s  volatility:  %s", prefix, get_volatility_name(get_entity_volatility(ent)));
		fprintf(F, "\n%s  aligned:  %s", prefix, get_align_name(get_entity_aligned(ent)));
		fprintf(F, "\n%s  alignment:  %u", prefix, get_entity_alignment(ent));
		fprintf(F, "\n%s  ld_name: %s", prefix, ent->ld_name ? get_entity_ld_name(ent) : "no yet set");
		fprintf(F, "\n%s  offset:  %d bytes, %d rem bits", prefix, get_entity_offset(ent), get_entity_offset_bits_remainder(ent));
		if (is_Method_type(get_entity_type(ent))) {
			if (get_entity_irg(ent))   /* can be null */ {
				fprintf(F, "\n%s  irg = %ld", prefix, get_irg_graph_nr(get_entity_irg(ent)));
			} else {
				fprintf(F, "\n%s  irg = NULL", prefix);
			}
		}
		fputc('\n', F);
	}
}

void dump_entity_to_file(FILE *out, ir_entity *ent)
{
	dump_entity_to_file_prefix(out, ent, "");
	fprintf(out, "\n");
}

void dump_type_to_file(FILE *F, ir_type *tp)
{
	size_t i;

	if ((is_Class_type(tp))       && (verbosity & dump_verbosity_noClassTypes)) return;
	if ((is_Struct_type(tp))      && (verbosity & dump_verbosity_noStructTypes)) return;
	if ((is_Union_type(tp))       && (verbosity & dump_verbosity_noUnionTypes)) return;
	if ((is_Array_type(tp))       && (verbosity & dump_verbosity_noArrayTypes)) return;
	if ((is_Pointer_type(tp))     && (verbosity & dump_verbosity_noPointerTypes)) return;
	if ((is_Method_type(tp))      && (verbosity & dump_verbosity_noMethodTypes)) return;
	if ((is_Primitive_type(tp))   && (verbosity & dump_verbosity_noPrimitiveTypes)) return;
	if ((is_Enumeration_type(tp)) && (verbosity & dump_verbosity_noEnumerationTypes)) return;

	ir_fprintf(F, "%+F", tp);
	if (verbosity & dump_verbosity_onlynames) { fprintf(F, "\n"); return; }

	switch (get_type_tpop_code(tp)) {

	case tpo_class:
		if ((verbosity & dump_verbosity_methods) || (verbosity & dump_verbosity_fields)) {
			fprintf(F, "\n  members:\n");
		}
		for (i = 0; i < get_class_n_members(tp); ++i) {
			ir_entity *mem = get_class_member(tp, i);
			if (((verbosity & dump_verbosity_methods) &&  is_Method_type(get_entity_type(mem))) ||
				((verbosity & dump_verbosity_fields)  && !is_Method_type(get_entity_type(mem)))   ) {
				if (!(verbosity & dump_verbosity_nostatic)) {
					dump_entity_to_file_prefix(F, mem, "    ");
				}
			}
		}
		if (verbosity & dump_verbosity_typeattrs) {
			fprintf(F, "  supertypes: ");
			for (i = 0; i < get_class_n_supertypes(tp); ++i) {
				ir_type *stp = get_class_supertype(tp, i);
				ir_fprintf(F, "\n    %d %+F", i, stp);
			}
			fprintf(F, "\n  subtypes: ");
			for (i = 0; i < get_class_n_subtypes(tp); ++i) {
				ir_type *stp = get_class_subtype(tp, i);
				ir_fprintf(F, "\n    %d %+F", i, stp);
			}

			if (get_irp_inh_transitive_closure_state() != inh_transitive_closure_none) {
				ir_type *stp;
				fprintf(F, "\n  transitive supertypes: ");
				for (stp = get_class_trans_supertype_first(tp);
				stp;
				stp = get_class_trans_supertype_next(tp)) {
					ir_fprintf(F, "\n    %+F", stp);
				}
				fprintf(F, "\n  transitive subtypes: ");
				for (stp = get_class_trans_subtype_first(tp);
				stp;
				stp = get_class_trans_subtype_next(tp)) {
					ir_fprintf(F, "\n    %+F", stp);
				}
			}

			fprintf(F, "\n  flags:       ");
			if (is_class_final(tp))
				fprintf(F, "final, ");
			if (is_class_interface(tp))
				fprintf(F, "interface, ");
			if (is_class_abstract(tp))
				fprintf(F, "abstract, ");
			fprintf(F, "\n");
		}
		break;

	case tpo_union:
	case tpo_struct:
		if (verbosity & dump_verbosity_fields) fprintf(F, "\n  members: ");
		for (i = 0; i < get_compound_n_members(tp); ++i) {
			ir_entity *mem = get_compound_member(tp, i);
			if (verbosity & dump_verbosity_fields) {
				dump_entity_to_file_prefix(F, mem, "    ");
			}
		}
		break;

	case tpo_array:
		if (verbosity & dump_verbosity_typeattrs) {
			size_t n_dim;
			ir_type *elem_tp = get_array_element_type(tp);

			fprintf(F, "\n  array ");

			n_dim = get_array_n_dimensions(tp);
			for (i = 0; i < n_dim; ++i) {
				ir_node *lower, *upper;

				lower = get_array_lower_bound(tp, i);
				upper = get_array_upper_bound(tp, i);

				fprintf(F, "[");

				if (is_Const(lower)) {
					fprintf(F, "%ld .. ", get_tarval_long(get_Const_tarval(lower)));
				} else {
					dump_node_opcode(F, lower);
					fprintf(F, " %ld .. ", get_irn_node_nr(lower));
				}

				if (is_Const(upper)) {
					fprintf(F, "%ld]", get_tarval_long(get_Const_tarval(lower)));
				} else {
					dump_node_opcode(F, upper);
					fprintf(F, " %ld]", get_irn_node_nr(upper));
				}
			}
			ir_fprintf(F, " of <%+F>", elem_tp);

			fprintf(F, "\n  order: ");
			for (i = 0; i < n_dim; ++i)
				fprintf(F, "<%zu>", get_array_order(tp, i));

			fprintf(F, "\n");

			if (verbosity & dump_verbosity_fields) {
				dump_entity_to_file_prefix(F, get_array_element_entity(tp),
				                           "    ");
			}
		}
		break;

	case tpo_pointer:
		if (verbosity & dump_verbosity_typeattrs) {
			ir_type *tt = get_pointer_points_to_type(tp);
			ir_fprintf(F, "\n  points to %+F\n", tt);
		}
		break;

	case tpo_method:
		if (verbosity & dump_verbosity_typeattrs) {
			mtp_additional_properties mtp = get_method_additional_properties(tp);
			unsigned cconv = get_method_calling_convention(tp);
			fprintf(F, "\n  variadicity: %s", get_variadicity_name(get_method_variadicity(tp)));
			fprintf(F, "\n  return types: %lu",
			        (unsigned long) get_method_n_ress(tp));
			for (i = 0; i < get_method_n_ress(tp); ++i) {
				ir_type *rtp = get_method_res_type(tp, i);
				ir_fprintf(F, "\n    %+F", rtp);
			}

			fprintf(F, "\n  parameter types: %lu",
			        (unsigned long) get_method_n_params(tp));
			for (i = 0; i < get_method_n_params(tp); ++i) {
				ir_type *ptp = get_method_param_type(tp, i);
				ir_fprintf(F, "\n    %+F", ptp);
			}
			fprintf(F, "\n  properties:");
			if (mtp & mtp_property_const)
				fputs(" const", F);
			if (mtp & mtp_property_pure)
				fputs(" pure", F);
			if (mtp & mtp_property_noreturn)
				fputs(" noreturn", F);
			if (mtp & mtp_property_nothrow)
				fputs(" nothrow", F);
			if (mtp & mtp_property_naked)
				fputs(" naked", F);
			if (mtp & mtp_property_malloc)
				fputs(" malloc", F);
			if (mtp & mtp_property_returns_twice)
				fputs(" returns_twice", F);
			if (mtp & mtp_property_intrinsic)
				fputs(" intrinsic", F);
			if (mtp & mtp_property_runtime)
				fputs(" runtime", F);
			if (mtp & mtp_property_private)
				fputs(" private", F);
			if (mtp & mtp_property_has_loop)
				fputs(" has_Loop", F);

			fprintf(F, "\n  calling convention:");
			if (cconv & cc_reg_param)
				fputs(" regparam", F);
			if (cconv & cc_last_on_top)
				fputs(" last_on_top", F);
			if (cconv & cc_callee_clear_stk)
				fputs(" calle_clear_stk", F);
			if (cconv & cc_this_call)
				fputs(" this_call", F);
			if (cconv & cc_compound_ret)
				fputs(" compound_ret", F);
			if (cconv & cc_frame_on_caller_stk)
				fputs(" frame_on_caller_stk", F);
			if (cconv & cc_fpreg_param)
				fputs(" fpreg_param", F);

			if (get_method_variadicity(tp)) {
				fprintf(F, "\n    ...");
			}
			fprintf(F, "\n");
		}
		break;

	case tpo_primitive:
		if (verbosity & dump_verbosity_typeattrs) {
			ir_type *base_tp = get_primitive_base_type(tp);
			if (base_tp != NULL)
				ir_fprintf(F, "\n  base type: %+F", tp);
			fprintf(F, "\n");
		}
		break;

	case tpo_none:
	case tpo_unknown:
		fprintf(F, "\n");
		break;

	default:
		if (verbosity & dump_verbosity_typeattrs) {
			fprintf(F, ": details not implemented\n");
		}
	}

	fprintf(F, "  state:      %s,\n", get_type_state_name(get_type_state(tp)));
	fprintf(F, "  size:       %2u Bytes,\n", get_type_size_bytes(tp));
	fprintf(F, "  alignment:  %2u Bytes,\n", get_type_alignment_bytes(tp));
	if (is_atomic_type(tp) || is_Method_type(tp))
		fprintf(F, "  mode:       %s,\n",  get_mode_name(get_type_mode(tp)));

	fprintf(F, "\n\n");
}

void dump_types_as_text(FILE *out)
{
	size_t i, n_types = get_irp_n_types();

	for (i = 0; i < n_types; ++i) {
		ir_type *type = get_irp_type(i);
		dump_type_to_file(out, type);
	}
}

void dump_globals_as_text(FILE *out)
{
	ir_type *global_type = get_glob_type();
	size_t   n_members   = get_class_n_members(global_type);
	size_t   i;

	for (i = 0; i < n_members; ++i) {
		ir_entity *entity = get_class_member(global_type, i);
		dump_entity_to_file(out, entity);
	}
}
