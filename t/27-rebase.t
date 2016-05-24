#!perl

use Test::More;

use Git::Raw;
use File::Slurp::Tiny qw(write_file);
use File::Spec::Unix;

my $path = File::Spec::Unix -> rel2abs(File::Spec::Unix -> catfile('t/rebase_repo'));

my $repo = Git::Raw::Repository -> init($path, 0);

my $file  = $repo -> workdir . '.gitignore';
write_file($file, '');

my $index = $repo -> index;
$index -> add('.gitignore');
$index -> write;

my $tree = $index -> write_tree;
isa_ok $tree, 'Git::Raw::Tree';

my $config = $repo -> config;
my $name   = $config -> str('user.name');
my $email  = $config -> str('user.email');

my $me = Git::Raw::Signature -> new($name, $email, time(), 0);
my $commit = Git::Raw::Commit -> create($repo, "initial commit\n", $me, $me, [], $tree);
isa_ok $commit, 'Git::Raw::Commit';

my $branch = $commit -> annotated;
my $upstream = $commit -> annotated;
my $onto = $commit -> annotated;

my $rebase = Git::Raw::Rebase -> new($repo, $branch, $upstream, $onto);
isa_ok $rebase, 'Git::Raw::Rebase';

is $rebase -> operation_count, 0;

ok (!eval {$rebase -> current_operation});

is $repo -> state, "rebase_merge";
is $repo -> is_head_detached, 1;

$rebase -> abort;
isnt $repo -> state, "rebase_merge";

done_testing;
