#include "git2/sys/diff.h"

#include "repository.h"
	git_buf_clear(&actual);
	cl_git_pass(git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, git_diff_print_callback__to_buf, &actual));

	cl_assert_equal_s(expected, actual.ptr);

		"Hc$@<O00001\n" \
		"\n";
		"Kc${Nk-~s>u4FC%O\n" \
		"\n";
		"Kc${Nk-~s>u4FC%O\n" \
		"\n";
		"JfH567LIF3FM2!Fd\n" \
		"\n";
		"Oc%18D`@*{63ljhg(E~C7\n" \
		"\n";

void test_diff_binary__empty_for_no_diff(void)
{
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_oid id;
	git_commit *commit;
	git_tree *tree;
	git_diff *diff;
	git_buf actual = GIT_BUF_INIT;

	opts.flags = GIT_DIFF_SHOW_BINARY | GIT_DIFF_FORCE_BINARY;
	opts.id_abbrev = GIT_OID_HEXSZ;

	repo = cl_git_sandbox_init("renames");

	cl_git_pass(git_oid_fromstr(&id, "19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13"));
	cl_git_pass(git_commit_lookup(&commit, repo, &id));
	cl_git_pass(git_commit_tree(&tree, commit));

	cl_git_pass(git_diff_tree_to_tree(&diff, repo, tree, tree, &opts));
	cl_git_pass(git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, git_diff_print_callback__to_buf, &actual));

	cl_assert_equal_s("", actual.ptr);

	git_buf_free(&actual);
	git_diff_free(diff);
	git_commit_free(commit);
	git_tree_free(tree);
}

void test_diff_binary__index_to_workdir(void)
{
	git_index *index;
	git_diff *diff;
	git_patch *patch;
	git_buf actual = GIT_BUF_INIT;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	const char *expected =
		"diff --git a/untimely.txt b/untimely.txt\n" \
		"index 9a69d960ae94b060f56c2a8702545e2bb1abb935..1111d4f11f4b35bf6759e0fb714fe09731ef0840 100644\n" \
		"GIT binary patch\n" \
		"delta 32\n" \
		"nc%1vf+QYWt3zLL@hC)e3Vu?a>QDRl4f_G*?PG(-ZA}<#J$+QbW\n" \
		"\n" \
		"delta 7\n" \
		"Oc%18D`@*{63ljhg(E~C7\n" \
		"\n";

	opts.flags = GIT_DIFF_SHOW_BINARY | GIT_DIFF_FORCE_BINARY;
	opts.id_abbrev = GIT_OID_HEXSZ;

	repo = cl_git_sandbox_init("renames");
	cl_git_pass(git_repository_index(&index, repo));

	cl_git_append2file("renames/untimely.txt", "Oh that crazy Kipling!\r\n");

	cl_git_pass(git_diff_index_to_workdir(&diff, repo, index, &opts));

	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));

	cl_assert_equal_s(expected, actual.ptr);

	cl_git_pass(git_index_add_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_write(index));

	test_patch(
		"19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13",
		NULL,
		&opts,
		expected);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);
	git_index_free(index);
}

static int print_cb(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	const git_diff_line *line,
	void *payload)
{
	git_buf *buf = (git_buf *)payload;

	GIT_UNUSED(delta);

	if (hunk)
		git_buf_put(buf, hunk->header, hunk->header_len);

	if (line)
		git_buf_put(buf, line->content, line->content_len);

	return git_buf_oom(buf) ? -1 : 0;
}

void test_diff_binary__print_patch_from_diff(void)
{
	git_index *index;
	git_diff *diff;
	git_buf actual = GIT_BUF_INIT;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	const char *expected =
		"diff --git a/untimely.txt b/untimely.txt\n" \
		"index 9a69d960ae94b060f56c2a8702545e2bb1abb935..1111d4f11f4b35bf6759e0fb714fe09731ef0840 100644\n" \
		"GIT binary patch\n" \
		"delta 32\n" \
		"nc%1vf+QYWt3zLL@hC)e3Vu?a>QDRl4f_G*?PG(-ZA}<#J$+QbW\n" \
		"\n" \
		"delta 7\n" \
		"Oc%18D`@*{63ljhg(E~C7\n" \
		"\n";

	opts.flags = GIT_DIFF_SHOW_BINARY | GIT_DIFF_FORCE_BINARY;
	opts.id_abbrev = GIT_OID_HEXSZ;

	repo = cl_git_sandbox_init("renames");
	cl_git_pass(git_repository_index(&index, repo));

	cl_git_append2file("renames/untimely.txt", "Oh that crazy Kipling!\r\n");

	cl_git_pass(git_diff_index_to_workdir(&diff, repo, index, &opts));

	cl_git_pass(git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, print_cb, &actual));

	cl_assert_equal_s(expected, actual.ptr);

	git_buf_free(&actual);
	git_diff_free(diff);
	git_index_free(index);
}

struct diff_data {
	char *old_path;
	git_oid old_id;
	git_buf old_binary_base85;
	size_t old_binary_inflatedlen;
	git_diff_binary_t old_binary_type;

	char *new_path;
	git_oid new_id;
	git_buf new_binary_base85;
	size_t new_binary_inflatedlen;
	git_diff_binary_t new_binary_type;
};

static int file_cb(
	const git_diff_delta *delta,
	float progress,
	void *payload)
{
	struct diff_data *diff_data = payload;

	GIT_UNUSED(progress);

	if (delta->old_file.path)
		diff_data->old_path = git__strdup(delta->old_file.path);

	if (delta->new_file.path)
		diff_data->new_path = git__strdup(delta->new_file.path);

	git_oid_cpy(&diff_data->old_id, &delta->old_file.id);
	git_oid_cpy(&diff_data->new_id, &delta->new_file.id);

	return 0;
}

static int binary_cb(
	const git_diff_delta *delta,
	const git_diff_binary *binary,
	void *payload)
{
	struct diff_data *diff_data = payload;

	GIT_UNUSED(delta);

	git_buf_encode_base85(&diff_data->old_binary_base85,
		binary->old_file.data, binary->old_file.datalen);
	diff_data->old_binary_inflatedlen = binary->old_file.inflatedlen;
	diff_data->old_binary_type = binary->old_file.type;

	git_buf_encode_base85(&diff_data->new_binary_base85,
		binary->new_file.data, binary->new_file.datalen);
	diff_data->new_binary_inflatedlen = binary->new_file.inflatedlen;
	diff_data->new_binary_type = binary->new_file.type;

	return 0;
}

static int hunk_cb(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	void *payload)
{
	GIT_UNUSED(delta);
	GIT_UNUSED(hunk);
	GIT_UNUSED(payload);

	cl_fail("did not expect hunk callback");
	return 0;
}

static int line_cb(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	const git_diff_line *line,
	void *payload)
{
	GIT_UNUSED(delta);
	GIT_UNUSED(hunk);
	GIT_UNUSED(line);
	GIT_UNUSED(payload);

	cl_fail("did not expect line callback");
	return 0;
}

void test_diff_binary__blob_to_blob(void)
{
	git_index *index;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_blob *old_blob, *new_blob;
	git_oid old_id, new_id;
	struct diff_data diff_data = {0};

	opts.flags = GIT_DIFF_SHOW_BINARY | GIT_DIFF_FORCE_BINARY;
	opts.id_abbrev = GIT_OID_HEXSZ;

	repo = cl_git_sandbox_init("renames");
	cl_git_pass(git_repository_index__weakptr(&index, repo));

	cl_git_append2file("renames/untimely.txt", "Oh that crazy Kipling!\r\n");
	cl_git_pass(git_index_add_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_write(index));

	git_oid_fromstr(&old_id, "9a69d960ae94b060f56c2a8702545e2bb1abb935");
	git_oid_fromstr(&new_id, "1111d4f11f4b35bf6759e0fb714fe09731ef0840");

	cl_git_pass(git_blob_lookup(&old_blob, repo, &old_id));
	cl_git_pass(git_blob_lookup(&new_blob, repo, &new_id));

	cl_git_pass(git_diff_blobs(old_blob,
		"untimely.txt", new_blob, "untimely.txt", &opts,
		file_cb, binary_cb, hunk_cb, line_cb, &diff_data));

	cl_assert_equal_s("untimely.txt", diff_data.old_path);
	cl_assert_equal_oid(&old_id, &diff_data.old_id);
	cl_assert_equal_i(GIT_DIFF_BINARY_DELTA, diff_data.old_binary_type);
	cl_assert_equal_i(7, diff_data.old_binary_inflatedlen);
	cl_assert_equal_s("c%18D`@*{63ljhg(E~C7",
		diff_data.old_binary_base85.ptr);

	cl_assert_equal_s("untimely.txt", diff_data.new_path);
	cl_assert_equal_oid(&new_id, &diff_data.new_id);
	cl_assert_equal_i(GIT_DIFF_BINARY_DELTA, diff_data.new_binary_type);
	cl_assert_equal_i(32, diff_data.new_binary_inflatedlen);
	cl_assert_equal_s("c%1vf+QYWt3zLL@hC)e3Vu?a>QDRl4f_G*?PG(-ZA}<#J$+QbW",
		diff_data.new_binary_base85.ptr);

	git_blob_free(old_blob);
	git_blob_free(new_blob);

	git__free(diff_data.old_path);
	git__free(diff_data.new_path);

	git_buf_free(&diff_data.old_binary_base85);
	git_buf_free(&diff_data.new_binary_base85);
}