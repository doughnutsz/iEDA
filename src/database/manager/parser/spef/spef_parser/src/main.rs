mod spef_parser;

fn main() {
    let spef_file_str = "/home/immelon/projects/iPD/src/database/manager/parser/spef/spef-parser/aes.spef";
    // let spef_file_str = "/home/immelon/projects/iPD/src/database/manager/parser/spef/spef-parser/aes_simple.spef";
    // let spef_file_str = "/home/immelon/projects/scripts_test_ipd/nangate45_example.spef";
    // let spef_file_str = "/home/immelon/projects/scripts_test_ipd/skywater_aes_cipher_top.spef";
    let parse_result = spef_parser::parse_spef_file(spef_file_str);
    match parse_result {
        Ok(exchange_data) => {
            println!("{exchange_data:#?}");
            println!("Parsed {spef_file_str} successfully\n");
        }
        Err(err) => {
            println!("Error when paring {spef_file_str}, {err}");
        },
    }
}