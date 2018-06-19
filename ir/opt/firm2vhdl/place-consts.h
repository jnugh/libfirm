/**
 * @file
 * @brief    Replaces Const nodes with PinnedConst nodes in the block(s) in which they are needed.
 *           In case of Phis, places PinnedConsts in the appropriate predecessor.
 * @author   Andreas Fried
 *
 */

#include <firm.h>

void place_consts(ir_graph *irg);
