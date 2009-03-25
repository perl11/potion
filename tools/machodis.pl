#!/usr/bin/perl

#### MACHODIS
####
#### A Mach-O executable annotating disassembler.
####
#### Developed by   : Robert Klep  (robert AT klep DOT name)
#### Latest version : http://klep.name/software/machodis/
#### License        : BSD
####
#### $Id: machodis 1591 2008-01-16 14:35:42Z robert $

use strict;
use warnings;
use IO::File;
use Getopt::Long;

# some regexes we use
my $hx 	= qr/[0-9a-f]+/;
my $hxx	= qr/0x[0-9a-f]+/;

# formats we use
my %FORMATS = (
	'i386'	=> {
		'long'	=> [ '@<<<<<<<<  @>>>>>>>>>>>>>>>>>>   @<<<<<  @*', 65 ],
		'short'	=> [ '@<<<<<<<<   @<<<<<  @*', 40 ],
	},
	'ppc'		=> {
		'long'	=> [ '@<<<<<<<<     @>>>>>>>>      @<<<<  @*',			57 ],
		'short'	=> [ '@<<<<<<<<   @<<<<  @*',	 40 ],
	},
);

# Objective-C types and qualifiers used in signature-decoding
my $objctypes = {
	"c"	=>	"char",
	"i"	=>	"int",
	"s"	=>	"short",
	"l"	=>	"long",
	"q"	=>	"long long",
	"C"	=>	"unsigned char",
	"I"	=>	"unsigned int",
	"S"	=>	"unsigned short",
	"L"	=>	"unsigned long",
	"Q"	=>	"unsigned long long",
	"f"	=>	"float",
	"d"	=>	"double",
	"B"	=>	"BOOL",
	"v"	=>	"void",
	"*"	=>	"char *",
	"@"	=>	"id",
	"#"	=>	"Class",
	":"	=>	"SEL",
	"?"	=>	"??",
};
my $objcqualifiers = {
	"r" => "const",
	"n" => "in",
	"N" => "inout",
	"o" => "out",
	"O" => "bycopy",
	"R" => "byref",
	"V" => "oneway",
};
my $objcTypeRe = join "|", map { quotemeta } keys %$objctypes;
my $objcQualRe = join "|", map { quotemeta } keys %$objcqualifiers;

# constants
use constant {
	CSTRING	=> 1,
	OBJCMSG	=> 2,
	OBJCCLS	=> 3,
	LITERAL	=> 4,
};

# state-table used for tracking anonymous subs
use constant {
	IN_CODE							=> 1,
	SEEN_END_OF_SUB			=> 2,
	SEEN_ALIGNMENT_NOP	=> 3,
	SEEN_SUB						=> 4,
};

# set defaults
chomp(my $arch	= `arch`);
my 		$format		= "long";

# parse commandline options
GetOptions(
	"a|arch|architecture=s"	=> \$arch,
	"s|short"								=> sub { $format = "short" },
	"l|long"								=> sub { $format = "long" },
);
my $file = shift @ARGV
	or die <<EOB;
Usage: $0 [-asl] <executable>

  -a, --arch arch_type : use 'arch_type' for disassembling a fat binary
                         ('i386' or 'ppc', defaults to host architecture)
  -s, --short          : short output format
  -l, --long           : long output format  (default)

EOB

# sanitycheck file
die "$0: cannot find file '$file'.\n"					unless -e $file;
die "$0: supplied filename is not a file.\n" 	unless -f $file or -l $file;
die "$0: cannot open '$file' for reading.\n" 	unless open (my $fh, $file);

# check for Mach-O file
$fh->sysread(my $buf, 4);
my	$magic				= unpack "N", $buf;
my	$isuniversal	= 0xcafebabe == $magic;
my	$isppc				= 0xfeedface == $magic;
my	$isi386				= 0xcefaedfe == $magic;
die "$0: not a mach-o file.\n" unless $isuniversal or $isppc or $isi386;

# check for a fat binary (if not, $arch is useless)
$fh->sysread($buf, 4);
my $numarch	= unpack "N", $buf;
my $isfat		= $isuniversal && $numarch > 1;

# if binary isn't fat, use the architecture contained within
$arch	= $isi386 ? "i386" : "ppc" if not $isuniversal;

# architecture-dependent swap-function
my $swap = do {
	if ("ppc" eq $arch)
	{
		sub { $_[0] }
	}
	else
	{
		sub { unpack "N", pack "V", $_[0] }
	}
};

# make otool use architecture
my $OTOOL = $isfat ? "otool -arch $arch" : "otool";

# load cstring section
my $cstrings = {};
{
	open my $pipe, "$OTOOL -v -s __TEXT __cstring '$file' |"
		or die $!;
	while (<$pipe>)
	{
		next unless /^0/;
		chomp;
		my ($addr, $string) = split " ", $_, 2;
		$cstrings->{hex $addr} = $string;
	}
}

# cfstring section
{
	open my $pipe, "$OTOOL -X -s __DATA __cfstring '$file' |"
		or die $!;
	while (<$pipe>)
	{
		chomp;
		my ($addr, $bytestring) = split " ", $_, 2;

		$bytestring =~ s/\s//g;
		my $idx		  = &$swap(hex substr($bytestring, 16, 8));
		$cstrings->{hex $addr} = $cstrings->{$idx}
			if exists $cstrings->{$idx};
	}
}

# objc messagerefs
my $objcmsgrefs = {};
{
	open my $pipe, "$OTOOL -v -s __OBJC __message_refs '$file' |"
		or die $!;
	while (<$pipe>)
	{
		next unless /^0/;
		chomp;
		my ($addr, $selector) = $_ =~ m/^(.*?)\s+.*?__cstring:(.*)/;
		$objcmsgrefs->{hex $addr} = $selector;
	}
}

# objc classrefs
my $objcclsrefs = {};
{
	for my $which ("__cls_refs", "__class", "__meta_class")
	{
		open my $pipe, "$OTOOL -s __OBJC $which '$file' |"
			or die $!;
		while (<$pipe>)
		{
			next unless /^0/;
			chomp;
			my ($addr, $bytestring) = split " ", $_, 2;

			$bytestring =~ s/\s//g;
			my $i = 0;
			for my $byte ($bytestring =~ m/(.{8})/g)
			{
				my $idx = &$swap(hex $byte);
				$objcclsrefs->{hex($addr) + $i} = $cstrings->{$idx}
					if exists $cstrings->{$idx};
				$i += 4;
			}
		}
	}
}

# objc methodlists
my $objcmethsig = {};
{
	# read Objective-C segment
	open my $pipe, "$OTOOL -v -o '$file' |" 
		or die $!;
	my $text = do { local $/; <$pipe> };

	# split into class-blocks and shift off dummy block
	my @blocks	= split /defs\[\d+\]|Meta Class/, $text;
	shift @blocks;

	# process each class
	for my $str (@blocks)
	{
		# determine name ('class name' matches categories)
		my $clsname	= [ $str =~ m/^\s+(?:class\s+)?name\s+[a-f0-9x]+\s+(.*?)$/ms ]->[0];
		my $clsinfo	= [ $str =~ m/^\s+info\s+[a-f0-9x]+\s+(.*?)$/ms ]->[0] || "CLS_CLASS";

		# determine methods
		my @methods = $str =~ m{
			\s+method_name\s+[a-f0-9x]+\s+(.*?)\n
			\s+method_types\s+[a-f0-9x]+\s+(.*?)\n
			\s+(?:method_imp\s+([a-f0-9x]+)(.*?))\n
		}xgs;

		# process each method
		while (my ($mname, $msig, $maddr, $mfull) = splice @methods, 0, 4)
		{
			# strip whitespace
			$mfull =~ s/^\s+|\s+$//g;

			# make up our own full method signature if there isn't one
			unless (length $mfull)
			{
				# determine if this is a class- or object-method
				my $type 	= "CLS_META" eq $clsinfo ? "+" : "-";
				$mfull 		= "$type\[$clsname $mname]";
			}

			# decode signature
			my ($out, $in) = objcTypeDecode($msig);

			# add returntype to method signature if we could decode it
			my $offsets = [];
			if (defined $out->{type})
			{
				$mfull =~ s/^([+-])/"$1($out->{type})"/e;

				# process arguments
				my @parts = split ":", $mfull;
				for (my $i = 0; $i < @parts - 1; $i++)
				{
					$parts[$i] .= sprintf ":(%s)arg%i", $in->[$i]->{type} || "??", $i + 1;
					push @$offsets, $in->[$i]->{offset} || "??";
				}
				$mfull = join " ", @parts;
			}
			my $idx = 1;
			$objcmethsig->{hex $maddr} = {
				signature	=> $mfull,
				offsets		=> { map { $_ => $idx++ } @$offsets },
			};
		}
	}
}

# literals
my $literals	= {};
{
	for my $type ('__literal4', '__literal8')
	{
		open my $pipe, "$OTOOL -v -s __TEXT $type '$file' |"
			or die $!;
		while (<$pipe>)
		{
			next unless /^0/;
			chomp;
			my ($addr, $value) = $_ =~ m/^($hx).*?\((.*)\)/;
			$literals->{hex $addr} = $value;
		}
	}
}

# print text-section (but don't disassemble)
my $textSection = [];
my $base;
if ("long" eq $format)
{
	open my $pipe, "$OTOOL -X -t '$file' |"
		or die $!;
	while (<$pipe>)
	{
		chomp;

		# split into address and bytestring
		my @a 					= split " ";
		my $address 		= hex shift @a;
		my $bytestring	= join "", @a;

		# store base address
		$base = $address
			unless $base;

		# store in array
		my $i = 0;
		for my $byte ($bytestring =~ m/(.{2})/g)
		{
			$textSection->[$address - $base + $i++] = $byte;
		}
	}
}

# print annotated disassembly dump
my (@ppcregs, %i386regs);
my ($curobjcmsg, $curobjccls);
{
	my $pipe	= IO::Peekable->new("$OTOOL -tV '$file' | sed -e 's/\\./ ./g' | c++filt |")
		or die $!;

	my $state							= IN_CODE;

	my $wasmethoddeclared = 0;
	my $argumentoffsets		= {};
	my $next;
	while (my $this = $pipe->getline)
	{
		do { $next = $pipe->peek; } while defined $next && $next !~ /^0/;

		chomp($this); chomp($next ||= "");

		if ($this =~ /^0/)
		{
			my ($address, $asm) 			= split " ", $this, 2;
			my ($nextaddr, $nextasm)	= split " ", $next, 2;
			my $sym;

			$nextaddr ||= "";
			$nextasm 	||= "";

			# print method signature
			if (SEEN_SUB != $state && exists $objcmethsig->{hex $address})
			{
				printf "\n%s:\n", $objcmethsig->{hex $address}->{signature};
				$argumentoffsets = $objcmethsig->{hex $address}->{offsets};
				$state = SEEN_SUB;
			}

			# find anonymous routines
			if ($asm =~ m/^ret|jmpl|blr|hlt|jmp\s+\*/)
			{
				$state = SEEN_END_OF_SUB;
			}
			elsif ($asm =~ m/^nop/)
			{
				if (SEEN_END_OF_SUB != $state)
				{
					$state = IN_CODE;
				}
			}
			elsif (SEEN_END_OF_SUB == $state && 
				$asm =~ m/\b(?:pushl\s+%ebp)|(?:mfspr\s+r\d+,lr)/)
			{
				print "\n<anonymous subroutine>:\n";
			}
			else
			{
				$state = IN_CODE;
			}

			# show offsets of known arguments
			if (my ($offset) = $this =~ m/($hxx)\((?:%ebp|r\d+)\)/)
			{
				$sym = [ sprintf "\$arg%i", $argumentoffsets->{hex $offset} ]
					if exists $argumentoffsets->{hex $offset};
			}

			# find load-immediate instructions
			if (my ($reg, $val) = $asm =~ m/\blis\s+r(\d+),($hxx)/i)
			{
				$ppcregs[$reg] = hex($val) << 16;
			}

			# find addi instructions
			if (my ($dst, $src, $val) = $asm =~ m/\baddi\s+r(\d+),r(\d+),($hxx)/i)
			{
				$ppcregs[$dst] = ($ppcregs[$src] || 0) + hex $val;
				$sym 					 = resolve($ppcregs[$dst]);
			}

			# find lwz-instructions
			if (my ($dst, $val, $reg) = $asm =~ m/\blwz\s+r(\d+),($hxx)\(r(\d+)\)/i)
			{
				my $addr 	= ($ppcregs[$reg] || 0) + hex $val;
				$sym 			= resolve($addr);
			}

			# find EIP-transfers and accompanying stringloads
			# TODO: reset $i386regs{} when register is reused for other purposes
			if (my ($calladdr) = $asm =~ m/\bcalll\s+($hxx)/)
			{
				if (hex($calladdr) eq hex($nextaddr) && (my ($reg) = $nextasm =~ m/\bpopl\s+%(\S+)/))
				{
					$i386regs{$reg} = hex $calladdr;
				}
			}
			
			if (my ($offset, $reg) = $asm =~ m/\b(?:leal|movl)\s+($hxx)\(%(\S+)\)/)
			{
				$sym = resolve($i386regs{$reg} + hex $offset)
					if defined $i386regs{$reg};
			}

			# find integer-encoded stringliterals (at least 2 chars long)
			if ($asm =~ m/\$0x([0-9a-f]{4,})/)
			{
				my $fullmatch	= $&;
				my $str 			= pack("H*", $1);

				if ($str =~ m/^[[:print:]\x00]+$/)
				{
					$str	=~ s/\x00/\\0/g;
					$asm  =~ s/\$$hxx+/'$str'/;
					$sym	=	 [ $fullmatch ];
				}
			}

			# find cstrings/objc-messages
			if (not defined($sym) and my ($val) = $asm =~ m/($hxx)/)
			{
				$sym = resolve(hex $val);
			}

			# reformat symbolstubs
			if ($asm =~ s/\s+; symbol stub for:\s*(.*)//gsm)
			{
				my $stub						= $1;
				my ($opcode, $addr) = $asm =~ m/^(\S+)\s+($hxx)/;

				$asm = sprintf "%s\t%s", $opcode, $stub . "()";
				$sym = [ sprintf "@ %s", $addr ];
			}

			# replace branches to optimized objc_msgSend code
			if ("ppc" eq $arch && $asm =~ m/bla\s+0xfffeff00/)
			{
				$asm = "bla _objc_msgSend_rtp";
				$sym = [ "@ 0xfffeff00" ];
			}

			# dump disassembled line using a format
			{
				no warnings;

				# split into op and arg
				my ($op, $arg)	= split " ", $asm, 2;

				$^A = '';
				if ("short" eq $format)
				{
					formline $FORMATS{$arch}{short}[0], $address, $op, $arg;
				}
				else
				{
					# get opcodes
					my $numbytes	= hex($nextaddr) 	- hex($address);
					my $idx				= hex($address) 	- $base;
					my $opcodes		= join "", @$textSection[$idx..($idx + $numbytes - 1)];

					formline $FORMATS{$arch}{long}[0], $address, $opcodes, $op, $arg;
				}
				chomp(my $line = $^A);
				if (defined $sym)
				{
					# format symbol according to type
					if (CSTRING == $sym->[1])
					{
						$sym = "'$sym->[0]'";
					}
					elsif (OBJCMSG == $sym->[1])
					{
						$curobjcmsg	= $sym->[0];
						$sym 				= "[... $curobjcmsg]";
					}
					elsif (OBJCCLS == $sym->[1])
					{
						$curobjccls	= $sym->[0];
						$sym 				= "[$curobjccls ...]";
					}
					elsif (LITERAL == $sym->[1])
					{
						$sym 				= $sym->[0];
					}
					elsif ($line =~ m/_objc_msgSend/ && defined $curobjccls && defined $curobjcmsg)
					{
						$sym = "[$curobjccls $curobjcmsg] $sym->[0]";
						$curobjccls = $curobjcmsg = undef;
					}
					else
					{
						$sym = $sym->[0];
					}
					$line .= " " x ($FORMATS{$arch}{$format}[1] - length $line);
					$line .= " ; $sym";
				}
				printf "%s\n", $line;
			}
		}
		elsif (my ($def) = $this =~ /^(.*):$/)
		{
			$state = SEEN_SUB;
			print "\n";
			if ($next =~ m/^0/)
			{
				my ($addr, $rest) = split " ", $next, 2;

				$argumentoffsets = {};
				if (exists $objcmethsig->{hex $addr})
				{
					$def 							= $objcmethsig->{hex $addr}->{signature};
					$argumentoffsets 	= $objcmethsig->{hex $addr}->{offsets};
				}
			}
			print $def, ":\n";
		}
		else
		{
			print $this, "\n";
		}
	}
}

# resolve what is at $addr
sub resolve
{
	my $addr = shift;

	if (defined(my $s =    $cstrings->{$addr})) { return [ $s, CSTRING	] };
	if (defined(my $s = $objcmsgrefs->{$addr})) { return [ $s, OBJCMSG ] };
	if (defined(my $s = $objcclsrefs->{$addr})) { return [ $s, OBJCCLS ] };
	if (defined(my $s =    $literals->{$addr})) { return [ $s, LITERAL ] };
	return undef;
}

# See http://developer.apple.com/documentation/Cocoa/Conceptual/ObjectiveC/Articles/chapter_13_section_9.html#//apple_ref/doc/uid/TP30001163-CH9-TPXREF165
sub objcTypeDecode
{
	my $signature = shift;
	my $qual 			= "";
	my $ptr				= "";
	my $args			= [];

	$_ = $signature;
LOOP:
	{
		# qualifier
		if (/\G($objcQualRe)/gc)
		{
			$qual = $objcqualifiers->{$1} . " ";
		}
		else
		{
			$qual = "";
		}

		# pointers
		if (/\G\^/gc)
		{
			$ptr	= " *";
		}
		else
		{
			$ptr = "";
		}

		# simple types
		if (/\G($objcTypeRe)(\d+)/gc)
		{
			push @$args, {
				type		=> sprintf("%s%s%s", $qual, $objctypes->{$1}, $ptr),
				offset	=> $2,
			};
			redo LOOP;
		}

		# unsupported for now: arrays, structs, unions
		if (/\G\[.*?\](\d+)/gc)
		{ 
			push @$args, {
				type		=> sprintf("%s[]%s", $qual, $ptr),
				offset	=> $2,
			};
			redo LOOP;
		}
		if (/\G\{(.*?)=.*?\}(\d+)/gc)
		{
			push @$args, {
				type		=> sprintf("%sstruct %s%s", $qual, $1, $ptr),
				offset	=> $2,
			};
			redo LOOP;
		}
		if (/\G\((.*?)=*.*?\)(\d+)/gc)
		{
			push @$args, {
				type		=> sprintf("%sunion %s%s", $qual, $1, $ptr),
				offset	=> $2,
			};
			redo LOOP;
		}
	}

	# first is returntype
	my $out = shift @$args;

	# next are 'self' and '_cmd', the rest are method arguments
	shift @$args; shift @$args;

	# return to caller
	return ( $out, $args );
}

package IO::Peekable;
use IO::File;

sub new
{
	my $class = shift;
	my $self	= { handle => IO::File->new(@_), buffer => [], peekidx => 0 };
	return bless $self, $class;
}

sub peek
{
	my $self = shift;

	if (@{$self->{buffer}} <= $self->{peekidx}) { push @{$self->{buffer}}, $self->{handle}->getline };
	return $self->{buffer}->[$self->{peekidx}++];
}

sub reset
{
	my $self = shift;

	$self->{peekidx} = 0;
}

sub getline
{
	my $self = shift;

	$self->reset;
	if (@{$self->{buffer}}) { return shift @{$self->{buffer}} };
	return $self->{handle}->getline;
}

# vim:ts=2:sw=2:tw=0:
