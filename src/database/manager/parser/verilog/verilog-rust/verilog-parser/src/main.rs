mod verilog_parser;
use std::time::Instant;

fn main() {
    let start_time = Instant::now();

    let verilog_file_str =
        // "/home/longshuaiying/iEDA/src/database/manager/parser/verilog/verilog-rust/verilog-parser/example/example1.v";
    "/home/taosimin/T28/ieda_1208/asic_top_1208.syn.v";
    // "/home/longshuaiying/iEDA/scripts/design/sky130_gcd/result/verilog/gcd.v";

    let top_module_name = "asic_top";
    let mut verilog_file = verilog_parser::parse_verilog_file(verilog_file_str);
    verilog_parser::flatten_module(&mut verilog_file, top_module_name);
    let top_verilog_module_option = verilog_file.get_module(top_module_name);
    let top_module = top_verilog_module_option.unwrap().borrow();
    // println!("{:#?}", top_module);
    let inst_stmt = top_module.find_inst_stmt("u0_clk", "PDXOEDG_V_G");
    if let Some(inst_stmt_value) = inst_stmt {
        println!("{:#?}", inst_stmt_value);
    }
    // let dcls_stmt = top_module.find_dcls_stmt("u0_soc_top/u0_vga_ctrl/vga/buf11_addr");
    // println!("{:#?}", dcls_stmt);

    // println!("Number of verilog modules: {}", verilog_modules.len());

    // let verilog_modules = verilog_file.get_verilog_modules();
    // let _found_module = verilog_modules.iter().find(|&module| module.borrow().get_module_name() == top_module_name);

    let end_time = Instant::now();
    let elapsed_time = end_time.duration_since(start_time);
    let elapsed_s = elapsed_time.as_secs();

    println!("Program execution time (seconds): {} s", elapsed_s);
}
