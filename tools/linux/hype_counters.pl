#!/usr/bin/perl
#
#  Copyright (C) 2005 Michal Ostrowski <mostrows@watson.ibm.com>, IBM Corp.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#
#  $Id$
#
#  Kills running LPARs, returns them to "READY" state
#
use File::stat;
use File::Basename;
use IO::File;
use IO::Handle;
use strict;
use Getopt::Long;
use Pod::Usage;

my $libdir;
BEGIN{
  use Cwd 'abs_path';
  $libdir = abs_path(dirname($0)) . '/../lib/hype';
}

use lib "$libdir/perl";
use HypeConst;
use HypeUtil;

Getopt::Long::Configure qw(no_ignore_case);


sub freeze() {
  `hvcall H_DEBUG 0x5000`;
}


sub thaw() {
  `hvcall H_DEBUG 0x5002`;
}

if($#ARGV < 0) { exit(0);}

if($ARGV[0] eq 'freeze'){
  freeze();
}elsif($ARGV[0] eq 'thaw'){
  thaw();
} elsif($ARGV[0] eq 'set') {
  my $i = 0;
  shift @ARGV;
  while($#ARGV >= 0 && $i < 16) {
    my $arg = shift @ARGV;
    `hvcall H_DEBUG 0x5001 $i $arg`;
    ++$i;
  }
} elsif($ARGV[0] eq 'get') {
  my $i = 0;
  shift @ARGV;
  while($i < 16) {
    my @result = split /\s+/, `hvcall H_DEBUG 0x5003 $i`;
    my $num = $result[1];
    my $id = $result[2];
    $num =~ s/^0x0{0,15}/0x/;
    $id =~ s/^0x0{0,15}/0x/;

    printf("%02d: $id $num\n", $i);
    ++$i;
  }
}  
