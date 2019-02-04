#!/usr/bin/env perl
###############################################################################
##
## $Id: docpp.pl,v 1.5 2003/08/17 21:12:32 jasta Exp $
##
## Apply post-processing to the output HTML documentation from xsltproc.
##
## Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
##
## This program is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option) any
## later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
###############################################################################

use strict;
use Data::Dumper;

###############################################################################

my ($module, $description) = extract_docinfo (shift @ARGV);
printf ("<!-- %s: %s -->\n",
		$module      || 'No Module',
		$description || 'No Description');

while (<STDIN>)
{
	s% xmlns=\".*?\"%%g;
	s%<br></br>%<br />%g;

	print;
}

###############################################################################

sub extract_docinfo
{
	my $orig_in = shift;
	my $doc     = undef;
	my $descr   = undef;

	open (my $fhandle, $orig_in) or
	  die "Cannot open $orig_in: $!";

	local $/ = '-->';
	my $data = <$fhandle>;

	my ($hdr) = $data =~ m/<!--(.*?)-->/s;
	$hdr =~ s/^\#+ *//mg;

	my ($doc)   = $hdr =~ m/^Module:\s*(.*)$/m;
	my ($descr) = $hdr =~ m/^Description:\s*(.*?)\n\n/ms;

	close $fhandle;

	return fmt_text ($doc, $descr);
}

sub fmt_text
{
	foreach (@_)
	{
		s/^\s*//;
		s/\s*$//;
		s/\s+/ /g;
	}

	return @_;
}
