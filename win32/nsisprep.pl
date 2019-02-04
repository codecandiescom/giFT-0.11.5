#!/bin/perl
# $Id: nsisprep.pl,v 1.3 2003/12/12 03:19:38 jasta Exp $
use File::Path;

# this file must be run in $BUILD_ROOT\win32-dist
$TMP_DIR = "tmp";

mkpath($TMP_DIR);

# TODO: check for cygwin?

$LF = "\r\n";

#UNAME=`uname | cut -c 1-6`
#if [ $UNAME = CYGWIN ]; then
#LF=\\n
#else
#LF=\\r\\n
#fi

&change_eol ("README",  "$TMP_DIR/README");
&change_eol ("AUTHORS", "$TMP_DIR/AUTHORS");
&change_eol ("COPYING", "$TMP_DIR/COPYING");
&change_eol ("NEWS",    "$TMP_DIR/NEWS");

&change_eol ("data/Gnutella/Gnutella.conf",   "$TMP_DIR/Gnutella.conf");
&change_eol ("data/FastTrack/FastTrack.conf", "$TMP_DIR/FastTrack.conf");

&fixup_openftconf ("data/OpenFT/OpenFT.conf", "$TMP_DIR/OpenFT.conf");
&fixup_giftdconf  ("giftd.conf",              "$TMP_DIR/giftd.conf");


sub fixup_openftconf
{
	my $src = @_[0];
	my $dst = @_[1];

	if (not open (SRC, "<$src"))
	{
		print STDERR "WARNING: couldn't open $src for reading\n";
		return;
	}
	binmode SRC;

	if (not open (DST, ">$dst"))
	{
		print STDERR "WARNING: couldn't open $dst for writing\n";
		close SRC;
		return;
	}
	binmode DST;

	while (<SRC>)
	{
		s/\r?\n//;

		s/^\s*port\s*=.*/port = 0/;
		s/^\s*http_port\s*=.*/http_port = 1216/;
		s/^\s*env_path\s*=.*/env_path = OpenFT\/db/;

		print DST "$_$LF";
	}

	close DST;
	close SRC;
}

sub fixup_giftdconf
{
	my $src = @_[0];
	my $dst = @_[1];

	if (not open (SRC, "<$src"))
	{
		print STDERR "WARNING: couldn't open $src for reading\n";
		return;
	}
	binmode SRC;

	if (not open (DST, ">$dst"))
	{
		print STDERR "WARNING: couldn't open $dst for writing\n";
		close SRC;
		return;
	}
	binmode DST;

	while (<SRC>)
	{
		s/\r?\n//;

		s/^\s*incoming\s*=\s*~\/\.giFT\/incoming/incoming = \/C\/Program Files\/giFT\/incoming/;
		s/^\s*completed\s*=\s*~\/\.giFT\/completed/completed = \/C\/Program Files\/giFT\/completed/;

		print DST "$_$LF";
	}

	close DST;
	close SRC;
}

sub change_eol
{
	my $src = @_[0];
	my $dst = @_[1];

	if (not open (FILE, "<$src"))
	{
		print STDERR "WARNING: couldn't open $src for reading\n";
		return;
	}
	binmode FILE;
	local $/;
	my $data = <FILE>;
	close FILE;

	$data =~ s/\r?\n/$LF/g;

	if (not open (FILE, ">$dst"))
	{
		print STDERR "WARNING: couldn't open $dst for writing\n";
		return;
	}
	binmode FILE;
	print FILE $data;
	close FILE;
}