pub mod liberty_c_api;
pub mod liberty_data;

use pest::iterators::Pair;
use pest::Parser;
use pest_derive::Parser;

use std::collections::VecDeque;

use std::ffi::c_void;
use std::os::raw::c_char;

use self::liberty_data::{LibertyAttrValue, LibertyGroupStmt};

#[derive(Parser)]
#[grammar = "liberty_parser/grammar/liberty.pest"]
pub struct LibertyParser;

/// process float data.
fn process_float(pair: Pair<Rule>) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    let pair_clone = pair.clone();
    match pair.as_str().parse::<f64>() {
        Ok(value) => Ok(liberty_data::LibertyParserData::Float(liberty_data::LibertyFloatValue { value })),
        Err(_) => Err(pest::error::Error::new_from_span(
            pest::error::ErrorVariant::CustomError { message: "Failed to parse float".into() },
            pair_clone.as_span(),
        )),
    }
}

/// process string data.
fn process_string(pair: Pair<Rule>) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    let pair_clone = pair.clone();

    // println!("Rule:    {:?}", pair_clone.as_rule());
    // println!("Span:    {:?}", pair_clone.as_span());
    // println!("Text:    {}", pair_clone.as_str());

    match pair.as_str().parse::<String>() {
        Ok(value) => Ok(liberty_data::LibertyParserData::String(liberty_data::LibertyStringValue {
            value: value.trim_matches('"').to_string(),
        })),
        Err(_) => Err(pest::error::Error::new_from_span(
            pest::error::ErrorVariant::CustomError { message: "Failed to parse float".into() },
            pair_clone.as_span(),
        )),
    }
}

/// prcess simple attribute expr token such as: 1nW, 1.9, "test"
fn process_expr_token(
    pair: Pair<Rule>,
    parser_queue: &mut VecDeque<liberty_data::LibertyParserData>,
) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    let pair_clone = pair.clone();

    let attribute_value = parser_queue.pop_front().unwrap();
    match attribute_value {
        liberty_data::LibertyParserData::String(_) => Ok(attribute_value),
        liberty_data::LibertyParserData::Float(_) => {
            if parser_queue.is_empty() {
                Ok(attribute_value)
            } else {
                match pair.as_str().parse::<String>() {
                    Ok(value) => Ok(liberty_data::LibertyParserData::String(liberty_data::LibertyStringValue {
                        value: value.trim_matches('"').to_string(),
                    })),
                    Err(_) => Err(pest::error::Error::new_from_span(
                        pest::error::ErrorVariant::CustomError { message: "Failed to parse float".into() },
                        pair_clone.as_span(),
                    )),
                }
            }
        }
        _ => Err(pest::error::Error::new_from_span(
            pest::error::ErrorVariant::CustomError { message: "Unknown rule".into() },
            pair.as_span(),
        )),
    }
}

/// process simple attribute
fn process_simple_attribute(
    pair: Pair<Rule>,
    parser_queue: &mut VecDeque<liberty_data::LibertyParserData>,
) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    let file_name = "tbd";
    // let line_no = pair.as_span().start_pos().line_col().0;
    let line_no = pair.line_col().0;
    if let liberty_data::LibertyParserData::String(liberty_string_value) = parser_queue.pop_front().unwrap() {
        let lib_id = &liberty_string_value.value;
        let attribute_value = parser_queue.pop_front().unwrap();
        match attribute_value {
            liberty_data::LibertyParserData::String(s) => {
                let simple_stmt = liberty_data::LibertySimpleAttrStmt::new(
                    file_name,
                    line_no,
                    lib_id,
                    Box::new(s) as Box<dyn liberty_data::LibertyAttrValue>,
                );
                Ok(liberty_data::LibertyParserData::SimpleStmt(simple_stmt))
            }
            liberty_data::LibertyParserData::Float(f) => {
                let simple_stmt = liberty_data::LibertySimpleAttrStmt::new(
                    file_name,
                    line_no,
                    lib_id,
                    Box::new(f) as Box<dyn liberty_data::LibertyAttrValue>,
                );
                Ok(liberty_data::LibertyParserData::SimpleStmt(simple_stmt))
            }
            _ => Err(pest::error::Error::new_from_span(
                pest::error::ErrorVariant::CustomError { message: "Unknown rule".into() },
                pair.as_span(),
            )),
        }
    } else {
        Err(pest::error::Error::new_from_span(
            pest::error::ErrorVariant::CustomError { message: "Unknown rule".into() },
            pair.as_span(),
        ))
    }
}

/// process complex attribute
fn process_complex_attribute(
    pair: Pair<Rule>,
    parser_queue: &mut VecDeque<liberty_data::LibertyParserData>,
) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    let file_name = "tbd";
    let line_no = pair.line_col().0;
    let mut attri_values: Vec<Box<dyn liberty_data::LibertyAttrValue>> = Vec::new();
    if let liberty_data::LibertyParserData::String(liberty_string_value) = parser_queue.pop_front().unwrap() {
        let lib_id = &liberty_string_value.value;
        for _ in 0..parser_queue.len() {
            let attribute_value = parser_queue.pop_front().unwrap();
            match attribute_value {
                liberty_data::LibertyParserData::String(s) => attri_values.push(Box::new(s)),
                liberty_data::LibertyParserData::Float(f) => attri_values.push(Box::new(f)),
                _ => todo!(),
            }
        }
        let complex_stmt = liberty_data::LibertyComplexAttrStmt::new(file_name, line_no, lib_id, attri_values);
        Ok(liberty_data::LibertyParserData::ComplexStmt(complex_stmt))
    } else {
        Err(pest::error::Error::new_from_span(
            pest::error::ErrorVariant::CustomError { message: "Unknown rule".into() },
            pair.as_span(),
        ))
    }
}

fn process_group_attribute(
    pair: Pair<Rule>,
    parser_queue: &mut VecDeque<liberty_data::LibertyParserData>,
) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    let file_name = "tbd";
    let line_no = pair.line_col().0;
    let mut attri_values: Vec<Box<dyn liberty_data::LibertyAttrValue>> = Vec::new();
    let mut stmts: Vec<Box<dyn liberty_data::LibertyStmt>> = Vec::new();
    if let liberty_data::LibertyParserData::String(liberty_string_value) = parser_queue.pop_front().unwrap() {
        let lib_id = &liberty_string_value.value;
        for _ in 0..parser_queue.len() {
            let attribute_value_or_stmt = parser_queue.pop_front().unwrap();
            match attribute_value_or_stmt {
                liberty_data::LibertyParserData::String(s) => attri_values.push(Box::new(s)),
                liberty_data::LibertyParserData::Float(f) => attri_values.push(Box::new(f)),
                liberty_data::LibertyParserData::SimpleStmt(simple_stmt) => stmts.push(Box::new(simple_stmt)),
                liberty_data::LibertyParserData::ComplexStmt(complex_stmt) => stmts.push(Box::new(complex_stmt)),
                liberty_data::LibertyParserData::GroupStmt(group_stmt) => stmts.push(Box::new(group_stmt)),
                _ => todo!(),
            }
        }
        let group_stmt = liberty_data::LibertyGroupStmt::new(file_name, line_no, lib_id, attri_values, stmts);
        Ok(liberty_data::LibertyParserData::GroupStmt(group_stmt))
    } else {
        Err(pest::error::Error::new_from_span(
            pest::error::ErrorVariant::CustomError { message: "Unknown rule".into() },
            pair.as_span(),
        ))
    }
}

/// process pest pair data.
fn process_pair(
    pair: Pair<Rule>,
    parser_queue: &mut VecDeque<liberty_data::LibertyParserData>,
) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    let pair_clone = pair.clone();
    let current_queue_len = parser_queue.len();
    for inner_pair in pair.into_inner() {
        let pair_result = process_pair(inner_pair, parser_queue);
        parser_queue.push_back(pair_result.unwrap());
    }

    println!("Rule:    {:?}", pair_clone.as_rule());
    println!("Span:    {:?}", pair_clone.as_span());
    println!("Text:    {}", pair_clone.as_str());

    let mut substitute_queue: VecDeque<liberty_data::LibertyParserData> = VecDeque::new();
    while parser_queue.len() > current_queue_len {
        substitute_queue.push_front(parser_queue.pop_back().unwrap());
    }

    match pair_clone.as_rule() {
        Rule::float => process_float(pair_clone),
        Rule::multiline_string => process_string(pair_clone),
        Rule::id => process_string(pair_clone),
        Rule::expr_token => process_expr_token(pair_clone, &mut substitute_queue),
        Rule::simple_attribute => process_simple_attribute(pair_clone, &mut substitute_queue),
        Rule::complex_attribute => process_complex_attribute(pair_clone, &mut substitute_queue),
        Rule::group => process_group_attribute(pair_clone, &mut substitute_queue),
        _ => Err(pest::error::Error::new_from_span(
            pest::error::ErrorVariant::CustomError { message: "Unknown rule".into() },
            pair_clone.as_span(),
        )),
    }
}

pub fn parse_lib_file(lib_file_path: &str) -> Result<liberty_data::LibertyParserData, pest::error::Error<Rule>> {
    // Generate liberty.pest parser
    let input_str =
        std::fs::read_to_string(lib_file_path).unwrap_or_else(|_| panic!("Can't read file: {}", lib_file_path));
    let parse_result = LibertyParser::parse(Rule::lib_file, input_str.as_str());
    let mut parser_queue: VecDeque<liberty_data::LibertyParserData> = VecDeque::new();

    match parse_result {
        Ok(pairs) => {
            let lib_data = process_pair(pairs.into_iter().next().unwrap(), &mut parser_queue);
            assert_eq!(parser_queue.len(), 0);
            lib_data
        }
        Err(err) => {
            // Handle parsing error
            println!("Error: {}", err);
            Err(err.clone())
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_parse_lib(lib_path: *const c_char) -> *mut c_void {
    let c_str = unsafe { std::ffi::CStr::from_ptr(lib_path) };
    let r_str = c_str.to_string_lossy().into_owned();
    println!("r str {}", r_str);

    let lib_file = parse_lib_file(&r_str);
    if let liberty_data::LibertyParserData::GroupStmt(lib_file_group) = lib_file.unwrap() {
        let lib_file_pointer = Box::new(lib_file_group);

        let raw_pointer = Box::into_raw(lib_file_pointer);
        raw_pointer as *mut c_void
    } else {
        panic!("error type");
    }
}

#[no_mangle]
pub extern "C" fn rust_free_lib_group(c_lib_group: *mut LibertyGroupStmt) {
    let _: Box<liberty_data::LibertyGroupStmt> = unsafe { Box::from_raw(c_lib_group) };
}

#[cfg(test)]
mod tests {
    use pest::error;
    use pest::iterators::Pair;
    use pest::iterators::Pairs;

    use super::*;

    fn test_process_pair(pair: Pair<Rule>) {
        // A pair is a combination of the rule which matched and a span of input
        println!("Rule:    {:?}", pair.as_rule());
        println!("Span:    {:?}", pair.as_span());
        println!("Text:    {}", pair.as_str());

        for inner_pair in pair.into_inner() {
            test_process_pair(inner_pair);
        }
    }

    fn print_parse_result(parse_result: Result<Pairs<Rule>, pest::error::Error<Rule>>) {
        let parse_result_clone = parse_result.clone();
        match parse_result {
            Ok(pairs) => {
                for pair in pairs {
                    // A pair is a combination of the rule which matched and a span of input
                    test_process_pair(pair);
                }
            }
            Err(err) => {
                // Handle parsing error
                println!("Error: {}", err);
            }
        }

        assert!(!parse_result_clone.is_err());
    }

    fn test_process_parse_result(parse_result: Result<Pairs<Rule>, pest::error::Error<Rule>>) {
        let mut parser_queue: VecDeque<liberty_data::LibertyParserData> = VecDeque::new();

        match parse_result {
            Ok(pairs) => {
                for pair in pairs {
                    let data = process_pair(pair, &mut parser_queue);
                    println!("Error: {:#?}", data);
                }
            }
            Err(err) => {
                // Handle parsing error
                println!("Error: {}", err);
            }
        }
    }

    #[test]
    fn test_parse_comment() {
        let input_str = "/*test 
        test
        */";
        let parse_result = LibertyParser::parse(Rule::COMMENT, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_float() {
        let input_str = "0";
        let parse_result = LibertyParser::parse(Rule::float, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_lib_id() {
        let input_str = "A";
        let parse_result = LibertyParser::parse(Rule::lib_id, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_bus_id() {
        let input_str = "A[ 1 ]";
        let parse_result = LibertyParser::parse(Rule::bus_id, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_bus_id1() {
        let input_str = "A[1:2]";
        let parse_result = LibertyParser::parse(Rule::bus_id, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_bus_bus_id() {
        let input_str = "A[1][1:2]";
        let parse_result = LibertyParser::parse(Rule::bus_bus_id, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_line_comment() {
        let input_str = "//test";
        let parse_result = LibertyParser::parse(Rule::line_comment, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_multiline_string() {
        let input_str = r#""test",\
        "test""#;
        let parse_result = LibertyParser::parse(Rule::multiline_string, input_str);

        // let tokens = parse_result.unwrap().tokens();

        // for token in tokens {
        //     println!("token {:?}", token);
        // }
        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_multiline_string1() {
        let input_str = r#"\
        "0, 0.00030875, 0.000558125, 0.000795625, 0.00106875, 0.00135375, 0.00182875, 0.002375, 0.00274312, 0.00315875, 0.00363375, 0.00419187, 0.00558125, 0.00806313", \
        "0, 0.000845, 0.0015275, 0.0021775, 0.002925, 0.003705, 0.005005, 0.0065, 0.0075075, 0.008645, 0.009945, 0.0114725, 0.015275, 0.0220675", \
        "0, 0.001365, 0.0024675, 0.0035175, 0.004725, 0.005985, 0.008085, 0.0105, 0.0121275, 0.013965, 0.016065, 0.0185325, 0.024675, 0.0356475", \
        "0, 0.001885, 0.0034075, 0.0048575, 0.006525, 0.008265, 0.011165, 0.0145, 0.0167475, 0.019285, 0.022185, 0.0255925, 0.034075, 0.0492275", \
        "0, 0.0034775, 0.00628625, 0.00896125, 0.0120375, 0.0152475, 0.0205975, 0.02675, 0.0308963, 0.0355775, 0.0409275, 0.0472138, 0.0628625, 0.0908163", \
        "0, 0.00398125, 0.00719687, 0.0102594, 0.0137812, 0.0174563, 0.0235812, 0.030625, 0.0353719, 0.0407312, 0.0468563, 0.0540531, 0.0719687, 0.103972", \
        "0, 0.00768625, 0.0138944, 0.0198069, 0.0266063, 0.0337012, 0.0455263, 0.059125, 0.0682894, 0.0786363, 0.0904612, 0.104356, 0.138944, 0.200729", \
        "0, 0.0081575, 0.0147463, 0.0210213, 0.0282375, 0.0357675, 0.0483175, 0.06275, 0.0724763, 0.0834575, 0.0960075, 0.110754, 0.147463, 0.213036", \
        "0, 0.01612, 0.02914, 0.04154, 0.0558, 0.07068, 0.09548, 0.124, 0.14322, 0.16492, 0.18972, 0.21886, 0.2914, 0.42098", \
        "0, 0.0165263, 0.0298744, 0.0425869, 0.0572063, 0.0724612, 0.0978862, 0.127125, 0.146829, 0.169076, 0.194501, 0.224376, 0.298744, 0.431589", \
        "0, 0.0329875, 0.0596312, 0.0850062, 0.114187, 0.144637, 0.195387, 0.25375, 0.293081, 0.337487, 0.388237, 0.447869, 0.596312, 0.861481", \
        "0, 0.0332475, 0.0601013, 0.0856763, 0.115088, 0.145778, 0.196928, 0.25575, 0.295391, 0.340148, 0.391298, 0.451399, 0.601013, 0.868271", \
        "0, 0.0667062, 0.120584, 0.171897, 0.230906, 0.292481, 0.395106, 0.513125, 0.592659, 0.682456, 0.785081, 0.905666, 1.20584, 1.74206" \
   "#;

        let parse_result = LibertyParser::parse(Rule::multiline_string, input_str);
        test_process_parse_result(parse_result);
    }

    #[test]
    fn test_parse_simple_attribute() {
        let input_str = r#"leakage_power_unit : 1nW;"#;
        let parse_result = LibertyParser::parse(Rule::simple_attribute, input_str);

        test_process_parse_result(parse_result);
        // print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_complex_attribute() {
        let input_str = r"define(process_corner, operating_conditions, string);";
        let parse_result = LibertyParser::parse(Rule::complex_attribute, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_group_attribute() {
        let input_str = r#"operating_conditions("ssg0p81v125c"){
            process : 1; /* SSGlobalCorner_LocalMC_MOS_MOSCAP-SSGlobalCorner_LocalMC_RES_BIP_DIO_DISRES */
            temperature : 125;
            voltage : 0.81;
            tree_type : "balanced_tree";
        }"#;
        let parse_result = LibertyParser::parse(Rule::group, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_lib_file() {
        let input_str = r#"library (tcbn28hpcplusbwp30p140ulvtssg0p81v125c_ccs) {
            /*  library head: tcbn28hpcplusbwp30p140ulvt */
            operating_conditions("ssg0p81v125c"){
                process : 1; /* SSGlobalCorner_LocalMC_MOS_MOSCAP-SSGlobalCorner_LocalMC_RES_BIP_DIO_DISRES */
                temperature : 125;
                voltage : 0.81;
                tree_type : "balanced_tree";
            }
       }"#;
        let parse_result = LibertyParser::parse(Rule::lib_file, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_lib_file_path() {
        let lib_file_path =
            "/home/taosimin/iEDA/src/database/manager/parser/liberty/lib-rust/liberty-parser/example/example1_slow.lib";
        // let lib_file_path = "/home/taosimin/T28/ccslib/tcbn28hpcplusbwp30p140ulvtssg0p81v125c_ccs.lib";

        let input_str =
            std::fs::read_to_string(lib_file_path).unwrap_or_else(|_| panic!("Can't read file: {}", lib_file_path));
        let parse_result = LibertyParser::parse(Rule::lib_file, input_str.as_str());

        print_parse_result(parse_result);
    }
}
