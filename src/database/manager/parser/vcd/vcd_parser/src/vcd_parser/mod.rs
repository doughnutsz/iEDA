use pest::Parser;
use pest_derive::Parser;
use std::{cell::RefCell, collections::VecDeque};

use pest::iterators::Pair;

pub mod vcd_data;
use std::rc::Rc;

#[derive(Parser)]
#[grammar = "vcd_parser/grammar/grammar.pest"]
pub struct VCDParser;

/// process date.
fn process_date(pair: Pair<Rule>, vcd_file_parser: &mut vcd_data::VCDFileParser) {
    let date_text = pair.as_str().parse::<String>().unwrap();
    let vcd_file = vcd_file_parser.get_vcd_file();
    vcd_file.set_date(date_text);
}

/// process scale number.
fn process_scale_number(pair: Pair<Rule>, vcd_file_parser: &mut vcd_data::VCDFileParser) {
    let scale_number = pair.as_str().parse::<u32>().unwrap();
    let vcd_file = vcd_file_parser.get_vcd_file();
    vcd_file.set_time_resolution(scale_number);
}

/// process scale unit
fn process_scale_unit(pair: Pair<Rule>, vcd_file_parser: &mut vcd_data::VCDFileParser) {
    let scale_unit = pair.as_str().parse::<String>().unwrap();
    let vcd_file = vcd_file_parser.get_vcd_file();
    vcd_file.set_time_unit(&scale_unit);
}

/// process scale.
fn process_scale(pair: Pair<Rule>, vcd_file_parser: &mut vcd_data::VCDFileParser) {
    for inner_pair in pair.into_inner() {
        match inner_pair.as_rule() {
            Rule::scale_number => process_scale_number(inner_pair, vcd_file_parser),
            Rule::scale_unit => process_scale_unit(inner_pair, vcd_file_parser),
            _ => panic!("not process: rule {:?}", inner_pair.as_rule()),
        }
    }
}

/// process scale unit
fn process_comment(pair: Pair<Rule>, vcd_file_parser: &mut vcd_data::VCDFileParser) {
    let comment = pair.as_str().parse::<String>().unwrap();
    let vcd_file = vcd_file_parser.get_vcd_file();
    vcd_file.set_comment(comment);
}

/// process scope.
fn process_open_scope<'a>(pair: Pair<Rule>, vcd_file_parser: &mut vcd_data::VCDFileParser) {
    let mut inner_pairs = pair.into_inner().into_iter();
    let pair_type = inner_pairs.next().unwrap();
    let scope_type = pair_type.as_str();

    let pair_module = inner_pairs.next_back().unwrap();
    let module_name = pair_module.as_str();

    let mut new_scope = Box::new(vcd_data::VCDScope::new(String::from(module_name)));
    new_scope.set_scope_type(&scope_type);

    // if !vcd_file_parser.get_scope_stack().is_empty() {
    //     let parent_scope = scope_stack.back_mut().unwrap();
    //     new_scope.set_parent_scope(parent_scope);
    //     parent_scope.add_child_scope(new_scope);
    // }
}

/// process vcd data.
fn process_vcd(pair: Pair<Rule>, vcd_file_parser: &mut vcd_data::VCDFileParser) {
    let pair_clone = pair.clone();

    println!("Rule:    {:?}", pair_clone.as_rule());
    println!("Span:    {:?}", pair_clone.as_span());
    println!("Text:    {}", pair_clone.as_str());

    for inner_pair in pair.into_inner() {
        match inner_pair.as_rule() {
            Rule::date_text => process_date(inner_pair, vcd_file_parser),
            Rule::scale_text => process_scale(inner_pair, vcd_file_parser),
            Rule::comment_text => process_comment(inner_pair, vcd_file_parser),
            Rule::open_scope => process_open_scope(inner_pair, vcd_file_parser),

            _ => panic!("not process: rule {:?}", inner_pair.as_rule()),
        }
    }
}

/// process vcd file data.
pub fn parse_vcd_file(vcd_file_path: &str) -> Result<vcd_data::VCDFile, pest::error::Error<Rule>> {
    let input_str = std::fs::read_to_string(vcd_file_path)
        .unwrap_or_else(|_| panic!("Can't read file: {}", vcd_file_path));
    let parse_result = VCDParser::parse(Rule::vcd_file, input_str.as_str());

    match parse_result {
        Ok(pairs) => {
            let mut vcd_file_parser = vcd_data::VCDFileParser::new();
            process_vcd(pairs.into_iter().next().unwrap(), &mut vcd_file_parser);
            Ok(vcd_file_parser.get_vcd_file_imute())
        }
        Err(err) => {
            // Handle parsing error
            println!("Error: {}", err);
            Err(err.clone())
        }
    }
}

#[cfg(test)]
mod tests {
    use pest::error;
    use pest::iterators::Pair;
    use pest::iterators::Pairs;

    use super::*;

    fn process_pair(pair: Pair<Rule>) {
        // A pair is a combination of the rule which matched and a span of input
        println!("Rule:    {:?}", pair.as_rule());
        println!("Span:    {:?}", pair.as_span());
        println!("Text:    {}", pair.as_str());

        for inner_pair in pair.into_inner() {
            process_pair(inner_pair);
        }
    }

    fn print_parse_result(parse_result: Result<Pairs<Rule>, pest::error::Error<Rule>>) {
        let parse_result_clone = parse_result.clone();
        match parse_result {
            Ok(pairs) => {
                for pair in pairs {
                    // A pair is a combination of the rule which matched and a span of input
                    process_pair(pair);
                }
            }
            Err(err) => {
                // Handle parsing error
                println!("Error: {}", err);
            }
        }

        assert!(!parse_result_clone.is_err());
    }

    #[test]
    fn test_parse_unit() {
        let input_str = "ns";
        let parse_result = VCDParser::parse(Rule::scale_unit, input_str);

        print_parse_result(parse_result);
    }
    #[test]
    fn test_parse_number() {
        let input_str = "10";
        let parse_result = VCDParser::parse(Rule::scale_number, input_str);

        print_parse_result(parse_result);
    }
    #[test]
    fn test_parse_scale() {
        let input_str = "10ns";
        let parse_result = VCDParser::parse(Rule::scale_text, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_date() {
        let input_str = r#"$date
        Tue Aug 23 16:03:49 2022
    $end"#;
        let parse_result = VCDParser::parse(Rule::date, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_scalar_value_change() {
        let input_str = r#"#0
        $dumpvars
        bxxxx #
        0!
        1"
        $end
        #50
        1!
        b0000 #
        #100
        0"
        0!
        #150
        1!
        b0001 #
        #200
        0!
        #250
        1!
        b0010 #
        #300
        0!
        #350
        1!
        b0011 #
        #400
        0!
        #450
        1!
        b0100 #
        #500
        0!"#;
        let parse_result = VCDParser::parse(Rule::value_change_section, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_scope() {
        let input_str = r#"$date
        Tue Aug 23 16:03:49 2022
    $end
    
    $timescale
        1ns
    $end
    
    $comment Csum: 1 9ba2991b94438432 $end
    
    
    $scope module test $end
    
    $scope module top_i $end
    $var wire 1 ! clk $end
    $var wire 1 " reset $end
    $var wire 4 # out [3:0] $end
    
    $scope module sub_i $end
    $var wire 1 " reset $end
    $var wire 1 ! clk $end
    $var reg 4 # out [3:0] $end
    $upscope $end
    
    $upscope $end
    
    $upscope $end
    
    $enddefinitions $end"#;
        let parse_result = VCDParser::parse(Rule::vcd_file, input_str);

        print_parse_result(parse_result);
    }

    #[test]
    fn test_parse_vcd_file_path() {
        let vcd_path =
            "/home/taosimin/iEDA/src/database/manager/parser/vcd/vcd_parser/benchmark/test1.vcd";

        let input_str = std::fs::read_to_string(vcd_path)
            .unwrap_or_else(|_| panic!("Can't read file: {}", vcd_path));
        let parse_result = VCDParser::parse(Rule::vcd_file, input_str.as_str());

        print_parse_result(parse_result);
    }

    #[test]
    fn test_build_vcd_data() {
        let vcd_path =
            "/home/taosimin/iEDA/src/database/manager/parser/vcd/vcd_parser/benchmark/test1.vcd";
        let parse_result = parse_vcd_file(vcd_path);
        assert!(parse_result.is_err());
    }
}
