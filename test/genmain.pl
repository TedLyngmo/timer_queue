#!/usr/bin/perl

use strict;
use warnings;

my @tests = map { s,.*/(.*)\.cpp$,$1,r } @ARGV;
@tests = grep!/^main$/, @tests;

my @decl = map { s/(.*)/namespace $1 { int main(); }\n/r } @tests;
my $decl = join('', @decl);

my @calls = map { s/(.*)/run("$1", $1::main)/r } @tests;
my $calls = 'return ' . join('||', @calls) . ';';

my $prog = do { local $/; <DATA> };

$prog =~ s/<EXTERN>/$decl/;
$prog =~ s/<TESTS>/$calls/;

print "$prog";

__DATA__
/*
    This is a generated file - do not edit
*/
#include <iostream>

<EXTERN>

static bool run(const char *name, int(*test)()) {
    std::cout << "Running " << name << '\n';
    bool res = test();
    std::cout << "Verdict " << name << ": " << (res ? "FAIL" : "OK") << '\n';
    return res;
}

int main() {
    <TESTS>
}
