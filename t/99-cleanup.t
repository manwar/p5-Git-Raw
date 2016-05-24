#!perl

use Test::More;

use Cwd qw(abs_path);
use File::Path qw(rmtree);

my @repos = (qw(test_repo rebase_repo));
foreach my $repo (@repos) {
	my $path = abs_path('t').'/'.$repo;;

	rmtree $path;
	ok ! -e $path;
}

done_testing;
