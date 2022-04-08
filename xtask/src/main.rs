use icu_properties::{sets, GeneralCategory};
use icu_uniset::UnicodeSet;
use std::{env, fmt::Write, fs, process::Command};

fn main() {
    let task = env::args().nth(1);
    match task.as_ref().map(|s| s.as_str()) {
        Some("gen-lexer") => gen_lexer(),
        Some("gen-unicode") => gen_unicode(),
        _ => help(),
    }
}

const RE2RUST: &str = "./tools/re2rust";
const LEX_RE: &str = "crates/lexer/lex.re";
const LEX_RS: &str = "crates/lexer/src/gen/mod.rs";

const UNICODE_RE: &str = "crates/lexer/unicode.re";

const LOOKUP_RE: &str = "crates/lexer/lookup.re";
const LOOKUP_RS: &str = "crates/lexer/src/gen/lookup.rs";

fn re2rust(input: &str, output: &str) {
    println!("re2rust: {} -> {}", input, output);

    let output = Command::new(RE2RUST)
        .args([
            input, "-o", output, "-T", // tags
            "-8", // utf-8
            "-W", // warnings
            "-Werror", // errors
                  //"-d" // debug
        ])
        .output()
        .expect("failed to execute re2rust");

    if output.status.success() {
        println!("  SUCCESS");
    } else {
        println!(
            "{}",
            String::from_utf8(output.stderr).expect("re2rust printed non utf-8 garbage :(")
        );
        std::process::exit(1);
    }
}

fn write_re2c_set(output: &mut String, name: &str, set: &UnicodeSet) {
    write!(output, "{} = [", name).unwrap();
    for range in set.iter_ranges() {
        if range.start() == range.end() {
            write!(output, "\\U{:08x}", range.start()).unwrap();
        } else {
            write!(output, "\\U{:08x}-\\U{:08x}", range.start(), range.end()).unwrap();
        }
    }
    write!(output, "];\n").unwrap();
}

fn gen_re2c_unicode_table() {
    let provider = icu_testdata::get_provider();

    let zs_payload = sets::get_for_general_category(&provider, GeneralCategory::SpaceSeparator)
        .expect("unable to load general category from icu");
    let zs_data = zs_payload.get();
    let zs = &zs_data.inv_list;

    let id_start_payload = sets::get_id_start(&provider).expect("unable to get ID_Start");
    let id_start_data = id_start_payload.get();
    let id_start = &id_start_data.inv_list;

    let id_continue_payload = sets::get_id_continue(&provider).expect("unable to get ID_Continue");
    let id_continue_data = id_continue_payload.get();
    let id_continue = &id_continue_data.inv_list;

    let mut output: String = "/*!re2c\n".to_string();
    write_re2c_set(&mut output, "Zs", zs);
    write_re2c_set(&mut output, "ID_Start", id_start);
    write_re2c_set(&mut output, "ID_Continue", id_continue);
    write!(output, "*/").unwrap();

    println!("generated {}", UNICODE_RE);
    fs::write(UNICODE_RE, output).expect("unable to write to unicode file");
}

fn write_rust_set(output: &mut String, name: &str, set: &UnicodeSet) {
    let mut lookup = phf_codegen::Set::new();
    for range in set.iter_ranges() {
        for i in range {
            lookup.entry(i);
        }
    }
    write!(
        output,
        "pub static {}: phf::Set<u32> = {};\n",
        name,
        lookup.build()
    )
    .unwrap();
}

fn gen_rust_unicode_table() {
    let provider = icu_testdata::get_provider();

    let id_start_payload = sets::get_id_start(&provider).expect("unable to get ID_Start");
    let id_start_data = id_start_payload.get();
    let id_start = &id_start_data.inv_list;

    let id_continue_payload = sets::get_id_continue(&provider).expect("unable to get ID_Continue");
    let id_continue_data = id_continue_payload.get();
    let id_continue = &id_continue_data.inv_list;

    let mut output: String = "\n".to_string();
    write_rust_set(&mut output, "ID_START", id_start);
    write_rust_set(&mut output, "ID_CONTINUE", id_continue);
    write!(output, "\n").unwrap();

    println!("generated {}", LOOKUP_RE);
    fs::write(LOOKUP_RE, output).expect("unable to write to lookup file");
}

fn gen_unicode() {
    gen_re2c_unicode_table();
    gen_rust_unicode_table();
}

fn gen_lexer() {
    re2rust(LEX_RE, LEX_RS);
    re2rust(LOOKUP_RE, LOOKUP_RS);
}

fn help() {
    println!(
        r#"usage: cargo xtask [task]

Available Tasks:

  gen-lexer        generate lexer source
  gen-unicode      generate unicode tables for lexer
"#
    );
}
