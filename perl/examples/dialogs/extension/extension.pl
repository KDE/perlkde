#!/usr/bin/perl

use strict;
use warnings;
use blib;

use Qt;

use FindDialog;

sub main {
    my $app = Qt::Application( \@ARGV );
    my $dialog = FindDialog();
    exit $dialog->exec();
}

main();
