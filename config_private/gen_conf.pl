#!/usr/bin/perl 
#===============================================================================
#
#         FILE:  gen_conf.pl
#
#        USAGE:  ./gen_conf.pl 
#
#  DESCRIPTION:  
#
#      OPTIONS:  ---
# REQUIREMENTS:  ---
#         BUGS:  ---
#        NOTES:  ---
#       AUTHOR:  
#      COMPANY:  
#      VERSION:  1.0
#      CREATED:  11/05/2009 04:26:17 PM CET
#     REVISION:  ---
#===============================================================================

use strict;
use warnings;

use Getopt::Long;
use Pod::Usage;
use Net::Domain qw(hostdomain);

my $f = 1;
my $NUM_PHY_REP = 4;
my $NUM_PHY_CLI = 40;
my $num_clients = 1000;
my $do_pki = '';
my $help = 0;

GetOptions ("f=i" => \$f,
			"rep=i" => \$NUM_PHY_REP,
			"cli=i" => \$NUM_PHY_CLI,
			"numclients=i" => \$num_clients,
			"pki!" => \$do_pki,
			"help|?" => \$help,
);

if ($help) {
	pod2usage( { -verbose => 1, -exitstatus => 0 } );
}

print STDOUT "Producing the output for f = $f\n";
print STDOUT "\twith $NUM_PHY_REP physical machines for replicas\n";
print STDOUT "\twith $NUM_PHY_CLI physical machines for clients\n";
print STDOUT "\t\twith total $num_clients clients\n";
print STDOUT "\twill ".($do_pki?'':'not ')."generate the public key files\n";

my %files = (	
		quorum => "config_quorum_f_$f",
		quorum_clients => "config_quorum_f_${f}_clients",
		backup => "config_backup_BFT_f_$f",
		backup_clients => "config_backup_BFT_f_${f}_clients",
		chain  => "config_chain_f_$f",
		chain_clients  => "config_chain_f_${f}_clients",
	    );

my $num_replicas = 3*$f+1;

#my @replica-s = reverse qw(replica-9-1 replica-8-1 replica-7-1 replica-6-1 replica-5-1 replica-4-1 replica-3-1 replica-2-1 replica-1-1 replica-0-1);
#my @replicas_ip = reverse qw(10.1.2.11 10.1.2.10 10.1.2.9 10.1.2.8 10.1.2.7 10.1.2.6 10.1.2.5 10.1.2.4 10.1.2.3 10.1.2.2); 
#my @replicas = qw(replica0-1 replica1-1 replica2-1 replica3-1);
#my @replicas_ip = qw(10.1.2.9 10.1.2.8 10.1.2.7 10.1.2.6); 

my @replicas = ();
my @replicas_ip = ();
for my $n (0..$NUM_PHY_REP-1) {
	#next if $n == 0;
	#next if $n == 2;
	push @replicas, "replica-$n-1";
	push @replicas_ip, "10.1.2.".($n+2);
}

#my @clients = qw(client0 client1 client2 client3 client4 client5 client6 client7);
#my @clients_ip = qw(10.1.1.13 10.1.1.14 10.1.1.15 10.1.1.16 10.1.1.17 10.1.1.18 10.1.1.19 10.1.1.20); 
#my @clients = qw(client0 client1 client2 client3);
#my @clients_ip = qw(10.1.1.7 10.1.1.8 10.1.1.9 10.1.1.10);

#my @clients = qw(client0 client1 client2 client3 client4 client5 client6 client7 client8 client9 client10 client11 client12 client13 client14 client15 client16 client17       client18      client19       ) ;
#my @clients_ip = qw(10.1.1.8 10.1.1.14 10.1.1.15 10.1.1.16 10.1.1.17 10.1.1.18 10.1.1.19 10.1.1.20 10.1.1.21 10.1.1.22 10.1.1.23 10.1.1.24 10.1.1.25 10.1.1.26 10.1.1.27 10.1.1.28 10.1.1.29 10.1.1.30 10.1.1.31 10.1.1.32); 

my @clients = ();
my @clients_ip = ();
for my $n (0..$NUM_PHY_CLI-1) {
	#next if $n == 15;
	push @clients, "client-$n";
	push @clients_ip, "10.1.1.".(2+$NUM_PHY_REP+1+$n);
}

#my @clients = qw(client0 client1 client2 client3 client4 client5 client6 client7 client8 client9 client10 client11 client12 client13 client14 client15 client16 client17       client18      client19       ) ;
#my @clients_ip = qw(10.1.1.10 10.1.1.14 10.1.1.15 10.1.1.16 10.1.1.17 10.1.1.18 10.1.1.19 10.1.1.20 10.1.1.21 10.1.1.22 10.1.1.23 10.1.1.24 10.1.1.25 10.1.1.26 10.1.1.27 10.1.1.28 10.1.1.29 10.1.1.30 10.1.1.31 10.1.1.32); 
#my @clients = qw(node10-1 node1-1 node2-1 node3-1);
#my @clients_ip = qw(10.1.2.12 10.1.2.3 10.1.2.4 10.1.2.5); 
my $group_factor = 1; # how many clients go on one client machine, before switch to another machine
#my @clients = qw(sci6 sci7 sci8);
#my @clients_ip = qw(192.168.20.6 192.168.20.7 192.168.20.8); 
#my $group_factor = 4; # how many clients go on one client machine, before switch to another machine


my %start_ports = (	quorum => 3364,
					backup => 4363,
					chain  => 5364,
				);

my %client_ports = (	quorum => 3400,
						backup => 4400,
						chain  => 5400,
					);
    
my %group_ports = (	quorum => 3363,
			backup => 4363,
			chain  => 5363,
		    );

# the public and secret keys
my $pkey = 'bfaa873efc926cb91646a89e45f96582041e3eed35cde0ef60b5c006cfad883781ee807411b0df3c74dc3ebbbce59c21d67711c83ecf596357c23dba33da338fb5577179a3b6188c59590aa1301eb852c0e14fa9225c0b377fee944eb9fa110ad7a316269e4b13b153887426a347c7c3c5feb1e3107bac4c6e29327b3343c405';
my $skey = 'd3bf9ada150474e93d21a4818ccf40e97df94f565c0528973a7799fc3e9ee69e0561fff15631850e2c5b8f9accee851cfc170cd0193052d4f75dfee18ab1d24b e7b8885fa504355a686140181ae956e726e490ac2f905e52a78bea2ef16acef31788b827f35f0de1343766e6f2cbe44f436d7e5eceeb67791ccd296422fb50ef';

my $group_addr = '239.5.6.8';
my $backup_to = '1800000';

open FBACKUP, ">$files{backup}" or die;
open FBACKUPC, ">$files{backup_clients}" or die;
open FQUORUM, ">$files{quorum}" or die;
open FQUORUMC, ">$files{quorum_clients}" or die;
open FCHAIN, ">$files{chain}" or die;
open FCHAINC, ">$files{chain_clients}" or die;

$\ = "\n";
print FBACKUP 'generic';
print FBACKUP $f;
print FBACKUP $backup_to;
print FBACKUP ($num_clients + $num_replicas);
print FBACKUP "$group_addr $group_ports{backup}";

print FBACKUPC 'generic';
print FBACKUPC $f;
print FBACKUPC $backup_to;
print FBACKUPC ($num_clients + $num_replicas);
print FBACKUPC "$group_addr $group_ports{backup}";

print FQUORUM $f;
print FQUORUM ($num_clients + $num_replicas);
print FQUORUM "$group_addr $group_ports{quorum}";
 
print FQUORUMC $f;
print FQUORUMC ($num_clients + $num_replicas);
print FQUORUMC "$group_addr $group_ports{quorum}";
 
print FCHAIN $f;
print FCHAIN ($num_clients + $num_replicas);
print FCHAIN "$group_addr $group_ports{chain}";

print FCHAINC $f;
print FCHAINC ($num_clients + $num_replicas);
print FCHAINC "$group_addr $group_ports{chain}";

for (my $i=0; $i < $num_replicas; $i++) {
	my $ndx = $i %($#replicas+1);
	local $, = " ";
	print FBACKUP $replicas[$ndx], $replicas_ip[$ndx], $start_ports{backup}, $pkey;
	print FQUORUM $replicas[$ndx], $replicas_ip[$ndx], $start_ports{quorum}, $pkey;
	print FCHAIN $replicas[$ndx], $replicas_ip[$ndx], $start_ports{chain}, $start_ports{chain}+200, $pkey;
	my ($rn, $rip) = ($replicas[$ndx], $replicas_ip[$ndx]);
	$rn =~ s/-1$/-0/o;
	$rip =~ s/^10\.1\.2/10.1.1/o;
	print FBACKUPC $rn, $rip, $start_ports{backup}, $pkey;
	print FQUORUMC $rn, $rip, $start_ports{quorum}, $pkey;
	print FCHAINC $rn, $rip, $start_ports{chain}, $start_ports{chain}+200, $pkey;
}

my %client_ports_per_machine;
$client_ports_per_machine{backup} = [ map { $client_ports{backup} } 0..$#clients ];
$client_ports_per_machine{quorum} = [ map { $client_ports{quorum} } 0..$#clients ];
$client_ports_per_machine{chain} = [ map { $client_ports{chain} } 0..$#clients ];

for (my $i=0; $i < $num_clients; $i++) {
	my $ndx = int($i/$group_factor) % ($#clients + 1);
	my $cpbr = \$client_ports_per_machine{backup}->[$ndx];
	my $cpqr = \$client_ports_per_machine{quorum}->[$ndx];
	my $cpcr = \$client_ports_per_machine{chain}->[$ndx];

	local $, = " ";
	print FBACKUP $clients[$ndx], $clients_ip[$ndx], $$cpbr, $pkey;
	print FBACKUPC $clients[$ndx], $clients_ip[$ndx], $$cpbr, $pkey;
	print FQUORUM $clients[$ndx], $clients_ip[$ndx], $$cpqr, $pkey;
	print FQUORUMC $clients[$ndx], $clients_ip[$ndx], $$cpqr, $pkey;
	print FCHAIN $clients[$ndx], $clients_ip[$ndx], $$cpcr, $$cpcr+200, $pkey;
	print FCHAINC $clients[$ndx], $clients_ip[$ndx], $$cpcr, $$cpcr+200, $pkey;
	$$cpbr++;
	$$cpqr++;
	$$cpcr++;
}

print FBACKUP <<EOFB;
5000 
150
9999250000
EOFB

print FBACKUPC <<EOFB;
5000 
150
9999250000
EOFB

close FCHAINC;
close FCHAIN;
close FQUORUM;
close FQUORUMC;
close FBACKUP;
close FBACKUPC;


if ($do_pki) {
	# generate the secret key files
	$\=undef;
	print STDOUT "\tGenerating the secret keys:\n";
	my $network = hostdomain();
	for my $n (0..$NUM_PHY_REP-1) {
		my $fout = "replica-$n.$network";
		open FOUT, ">", $fout or die "Can't open '$fout': $!\n";
		print FOUT $skey,"\n";
		close FOUT;
		print STDOUT "\t\tgenerated the secret key for '$fout'\n";
	}
	for my $n (0..$NUM_PHY_CLI-1) {
		my $fout = "client-$n.$network";
		open FOUT, ">", $fout or die "Can't open '$fout': $!\n";
		print FOUT $skey,"\n";
		close FOUT;
		print STDOUT "\t\tgenerated the secret key for '$fout'\n";
	}
}

__END__
=head1 NAME

gen_conf.pl - Generate Abstract BFT configuration files

=head1 SYNOPSIS

perl gen_conf.pl [options]

Options:
    -help -?         brief help message
    -f N             set replication factor (f) to N
    -rep N           use N machines for replicas
    -cli N           use N machines for clients
    -numclients N    generate config for N total clients
    -pki -nopki      generate public key files

=head1 DESCRIPTION

B<This program> will generate the configuration files for
use in Abstract BFT.

=cut;
