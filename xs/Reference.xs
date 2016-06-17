MODULE = Git::Raw			PACKAGE = Git::Raw::Reference

SV *
annotated_commit(self)
	SV *self

	PREINIT:
		int rc;

		SV *repo;
		AnnotatedCommit commit;
		Repository repo_ptr;

	CODE:
		repo = GIT_SV_TO_MAGIC(self);
		repo_ptr = INT2PTR(Repository, SvIV((SV *) repo));

		rc = git_annotated_commit_from_ref(&commit,
			repo_ptr -> repository,
			GIT_SV_TO_PTR(Reference, self));
		git_check_error(rc);

		GIT_NEW_OBJ_WITH_MAGIC(
			RETVAL, "Git::Raw::AnnotatedCommit",
			commit, repo
		);

	OUTPUT: RETVAL

SV *
create(class, name, repo, object, ...)
	const char *class
	const char *name
	SV *repo
	SV *object

	PREINIT:
		int rc, force = 0;

		Repository repo_ptr;
		Reference ref;
		const git_oid *oid;

	CODE:
		if (items > 4)
			force = SvTRUE(ST(4));

		if (sv_isobject(object) &&
		    sv_derived_from(object, "Git::Raw::Blob"))
			oid = git_blob_id(GIT_SV_TO_PTR(Blob, object));
		else if (sv_isobject(object) &&
		         sv_derived_from(object, "Git::Raw::Commit"))
			oid = git_commit_id(GIT_SV_TO_PTR(Commit, object));
		else
			oid = git_tree_id(GIT_SV_TO_PTR(Tree, object));

		repo_ptr = GIT_SV_TO_PTR(Repository, repo);

		rc = git_reference_create(
			&ref, repo_ptr -> repository,
			name, oid, force, NULL
		);
		git_check_error(rc);

		GIT_NEW_OBJ_WITH_MAGIC(RETVAL, class, ref, SvRV(repo));

	OUTPUT: RETVAL

SV *
lookup(class, name, repo)
	SV *class
	SV *name
	SV *repo

	PREINIT:
		int rc;

		Repository repo_ptr;
		Reference ref;
		const char *ref_name;

	CODE:
		repo_ptr = GIT_SV_TO_PTR(Repository, repo);
		ref_name = git_ensure_pv(name, "name");

		rc = git_reference_lookup(
			&ref, repo_ptr -> repository,
			ref_name
		);

		if (rc == GIT_ENOTFOUND) {
			/* Try to "do what I mean" */
			rc = git_reference_dwim(
				&ref, repo_ptr -> repository,
				ref_name
			);
			if (rc == GIT_ENOTFOUND)
				RETVAL = &PL_sv_undef;
		}

		if (rc != GIT_ENOTFOUND) {
			git_check_error(rc);

			GIT_NEW_OBJ_WITH_MAGIC(
				RETVAL, SvPVbyte_nolen(class), ref, SvRV(repo)
			);
		}

	OUTPUT: RETVAL

void
delete(self)
	SV *self

	PREINIT:
		int rc;

	CODE:
		rc = git_reference_delete(GIT_SV_TO_PTR(Reference, self));
		git_check_error(rc);

		sv_setiv(SvRV(self), 0);

SV *
name(self)
	Reference self

	PREINIT:
		const char *msg;

	CODE:
		msg = git_reference_name(self);
		RETVAL = newSVpv(msg, 0);

	OUTPUT: RETVAL

SV *
type(self)
	Reference self

	PREINIT:
		SV *type = NULL;
		git_ref_t rt;

	CODE:
		rt = git_reference_type(self);

		switch (rt) {
			case GIT_REF_OID:
				type = newSVpv("direct", 0);
				break;

			case GIT_REF_SYMBOLIC:
				type = newSVpv("symbolic", 0);
				break;

			default:
				croak_assert("Unexpected reference type: %d", rt);
				break;
		}

		RETVAL = type;

	OUTPUT: RETVAL

SV *
owner(self)
	SV *self

	PREINIT:
		SV *repo;

	CODE:
		repo = GIT_SV_TO_MAGIC(self);
		RETVAL = newRV_inc(repo);

	OUTPUT: RETVAL

SV *
target(self, ...)
	SV *self

	PROTOTYPE: $;$

	PREINIT:
		int rc;
		Reference ref;

	CODE:
		ref = GIT_SV_TO_PTR(Reference, self);

		if (items == 2) {
			Reference new_ref;

			Commit commit = GIT_SV_TO_PTR(Commit, ST(1));

			rc = git_reference_set_target(&new_ref, ref, git_commit_id(commit), NULL);
			git_check_error(rc);

			GIT_NEW_OBJ_WITH_MAGIC(
				RETVAL, "Git::Raw::Reference",
				new_ref, GIT_SV_TO_MAGIC(self)
			);
		} else {
			git_ref_t rt = git_reference_type(ref);

			RETVAL = &PL_sv_undef;
			switch (rt) {
				case GIT_REF_OID: {
					git_object *obj;
					const git_oid *oid;

					oid = git_reference_target(ref);

					rc = git_object_lookup(
						&obj, git_reference_owner(ref),
						oid, GIT_OBJ_ANY
					);
					git_check_error(rc);

					RETVAL = git_obj_to_sv(
						obj, GIT_SV_TO_MAGIC(self)
					);
					break;
				}

				case GIT_REF_SYMBOLIC: {
					Reference linked_ref;
					const char *target;

					target = git_reference_symbolic_target(ref);

					rc = git_reference_lookup(
						&linked_ref,
						git_reference_owner(ref),
						target
					);
					git_check_error(rc);

					GIT_NEW_OBJ_WITH_MAGIC(
						RETVAL, "Git::Raw::Reference",
						linked_ref, GIT_SV_TO_MAGIC(self)
					);
					break;
				}

				default:
					croak_assert("Unexpected reference type: %d", rt);
					break;
			}
		}

	OUTPUT: RETVAL

SV *
peel(self, type)
	SV *self
	SV *type

	PREINIT:
		int rc;

		const char *type_str;
		git_otype t = GIT_OBJ_ANY;
		git_object *obj = NULL;

	CODE:
		type_str = git_ensure_pv(type, "type");

		if (strcmp(type_str, "commit") == 0)
			t = GIT_OBJ_COMMIT;
		else if (strcmp(type_str, "tree") == 0)
			t = GIT_OBJ_TREE;
		else if (strcmp(type_str, "tag") == 0)
			t = GIT_OBJ_TAG;
		else
			croak_usage("Invalid type for 'type'. Expected 'commit', 'tree' or 'tag'");

		rc = git_reference_peel(&obj,
			GIT_SV_TO_PTR(Reference, self),
			t
		);
		git_check_error(rc);

		RETVAL = git_obj_to_sv(obj, GIT_SV_TO_MAGIC(self));

	OUTPUT: RETVAL

SV *
shorthand(self)
	Reference self

	CODE:
		RETVAL = newSVpv(git_reference_shorthand(self), 0);

	OUTPUT: RETVAL

SV *
is_branch(self)
	Reference self

	CODE:
		RETVAL = newSViv(git_reference_is_branch(self));

	OUTPUT: RETVAL

SV *
is_remote(self)
	Reference self

	CODE:
		RETVAL = newSViv(git_reference_is_remote(self));

	OUTPUT: RETVAL

SV *
is_tag(self)
	Reference self

	PREINIT:
		int rc;

	CODE:
		RETVAL = newSViv(git_reference_is_tag(self));

	OUTPUT: RETVAL

SV *
is_note(self)
	Reference self

	CODE:
		RETVAL = newSViv(git_reference_is_note(self));

	OUTPUT: RETVAL

void
DESTROY(self)
	SV *self

	CODE:
		git_reference_free(GIT_SV_TO_PTR(Reference, self));
		SvREFCNT_dec(GIT_SV_TO_MAGIC(self));
