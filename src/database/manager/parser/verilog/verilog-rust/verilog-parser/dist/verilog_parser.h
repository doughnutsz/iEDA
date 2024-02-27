/*
Gutengerg Post Parser, the C bindings.

Warning, this file is autogenerated by `cbindgen`.
Do not modify this manually.

*/

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * The wire or port declaration.
 */
typedef enum DclType {
    KInput = 0,
    KInout = 1,
    KOutput = 2,
    KSupply0 = 3,
    KSupply1 = 4,
    KTri = 5,
    KWand = 6,
    KWire = 7,
    KWor = 8,
} DclType;

typedef struct VerilogModule VerilogModule;

typedef struct RustVec {
    void *data;
    uintptr_t len;
    uintptr_t cap;
    uintptr_t type_size;
} RustVec;

typedef struct RustVerilogID {
    char *id;
} RustVerilogID;

typedef struct RustVerilogIndexID {
    char *id;
    char *base_id;
    int32_t index;
} RustVerilogIndexID;

typedef struct RustVerilogSliceID {
    char *id;
    char *base_id;
    int32_t range_base;
    int32_t range_max;
} RustVerilogSliceID;

typedef struct RustVerilogNetIDExpr {
    uintptr_t line_no;
    const void *verilog_id;
} RustVerilogNetIDExpr;

typedef struct RustVerilogNetConcatExpr {
    uintptr_t line_no;
    struct RustVec verilog_id_concat;
} RustVerilogNetConcatExpr;

typedef struct RustVerilogConstantExpr {
    uintptr_t line_no;
    const void *verilog_id;
} RustVerilogConstantExpr;

typedef struct RustVerilogModule {
    uintptr_t line_no;
    char *module_name;
    struct RustVec port_list;
    struct RustVec module_stmts;
} RustVerilogModule;

typedef struct CRange {
    bool has_value;
    int32_t start;
    int32_t end;
} CRange;

typedef struct RustVerilogDcl {
    uintptr_t line_no;
    enum DclType dcl_type;
    char *dcl_name;
    struct CRange range;
} RustVerilogDcl;

typedef struct RustVerilogDcls {
    uintptr_t line_no;
    struct RustVec verilog_dcls;
} RustVerilogDcls;

typedef struct RustVerilogInst {
    uintptr_t line_no;
    char *inst_name;
    char *cell_name;
    struct RustVec port_connections;
} RustVerilogInst;

typedef struct RustVerilogPortRefPortConnect {
    const void *port_id;
    void *net_expr;
} RustVerilogPortRefPortConnect;

void *rust_parse_verilog(const char *verilog_path, const char *top_module_name);

void rust_free_verilog_module(struct VerilogModule *c_verilog_module);

uintptr_t rust_vec_len(const struct RustVec *vec);

void free_c_char(char *s);

struct RustVerilogID *rust_convert_verilog_id(void *c_verilog_virtual_base_id);

bool rust_is_id(void *c_verilog_virtual_base_id);

struct RustVerilogIndexID *rust_convert_verilog_index_id(void *c_verilog_virtual_base_id);

bool rust_is_bus_index_id(void *c_verilog_virtual_base_id);

const char *rust_get_index_name(struct RustVerilogSliceID *verilog_slice_id, uintptr_t index);

struct RustVerilogSliceID *rust_convert_verilog_slice_id(void *c_verilog_virtual_base_id);

bool rust_is_bus_slice_id(void *c_verilog_virtual_base_id);

struct RustVerilogNetIDExpr *rust_convert_verilog_net_id_expr(void *c_verilog_net_id_expr);

struct RustVerilogNetConcatExpr *rust_convert_verilog_net_concat_expr(void *c_verilog_net_concat_expr);

struct RustVerilogConstantExpr *rust_convert_verilog_constant_expr(void *c_verilog_constant_expr);

bool rust_is_id_expr(void *c_verilog_virtual_base_net_expr);

bool rust_is_concat_expr(void *c_verilog_virtual_base_net_expr);

bool rust_is_constant(void *c_verilog_virtual_base_net_expr);

struct RustVerilogModule *rust_convert_raw_verilog_module(struct VerilogModule *verilog_module);

struct RustVerilogDcl *rust_convert_verilog_dcl(void *c_verilog_dcl_struct);

struct RustVerilogDcls *rust_convert_verilog_dcls(void *c_verilog_dcls_struct);

struct RustVerilogInst *rust_convert_verilog_inst(void *c_verilog_inst);

struct RustVerilogPortRefPortConnect *rust_convert_verilog_port_ref_port_connect(void *c_port_connect);

bool rust_is_module_inst_stmt(void *c_verilog_stmt);

bool rust_is_module_assign_stmt(void *c_verilog_stmt);

bool rust_is_verilog_dcl_stmt(void *c_verilog_stmt);

bool rust_is_verilog_dcls_stmt(void *c_verilog_stmt);

bool rust_is_module_stmt(void *c_verilog_stmt);
