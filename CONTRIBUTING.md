# Contributing

Want to contribute? Great! You can do so through the standard GitHub pull
request model. For large contributions we do encourage you to file a ticket in
the GitHub issues tracking system prior to any code development to coordinate
with the Dynamic Simulation Environment development team early in the process.
Coordinating up front helps to avoid frustration later on.

Please do:

* Write contributions according to the prevailing coding style of the
  repository (see following ClangFormat section).
* Code documentation is written in [NumPy Style Python Docstrings][NumPyDoc].
* When the Super-Linter runs on your PR, be prepared to _correct_ any issues.
  The `Makefile` includes a `super-linter` target which you can run before
  submitting your PR.
* In general: sentences start with a Capital letter, end in a full stop, and
  are spell checked (EN-US, EN-GB or EN-AU).

Your contribution must be licensed under the Apache-2.0 license, the license
used by this project.


## Code Quality

### Linting

```bash
# Run the Super Linter.
$ make super-linter
...
[NOTICE]   All file(s) linted successfully with no errors detected
```


### Using ClangFormat

```bash
# Build the ClangFormat docker container (optional).
$ make docker
...
$ docker images
REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
clang-format        latest              1383c4b9a6ff        4 minutes ago       422MB
$ export DSE_CLANG_FORMAT_IMAGE=clang-format:latest

# Setup a command alias (using ghcr.io image).
$ export DSE_CLANG_FORMAT_IMAGE=ghcr.io/boschglobal/dse-clang-format:main
$ alias dse-clang-format='docker run --rm -it --volume $(pwd):/tmp/code ${DSE_CLANG_FORMAT_IMAGE}'

# Run ClangFormat.
$ dse-clang-format dse/clib/util/strings.c
Target is dse/clib/util/strings.c (file).
Using DSE Clang Format rules file.
Formatting dse/clib/util/strings.c
```


## Add / retain copyright notices

Include a copyright notice and license in each new file to be contributed,
consistent with the style used by this project. If your contribution contains
code under the copyright of a third party, document its origin, license, and
copyright holders.


## Sign your work

This project tracks patch provenance and licensing using the Developer
Certificate of Origin 1.1 (DCO) from [developercertificate.org][DCO] and
Signed-off-by tags initially developed by the Linux kernel project.

```text
Developer Certificate of Origin
Version 1.1
Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
1 Letterman Drive
Suite D4700
San Francisco, CA, 94129
Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.
Developer's Certificate of Origin 1.1
By making a contribution to this project, I certify that:
(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or
(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or
(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.
(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

With the sign-off in a commit message you certify that you authored the patch
or otherwise have the right to submit it under an open source license. The
procedure is simple: To certify above Developer's Certificate of
Origin 1.1 for your contribution just append a line

```text
Signed-off-by: Random J Developer <random@developer.example.org>
```

to every commit message using your real name or your pseudonym and a valid
email address.

If you have set your `user.name` and `user.email` git configs you can
automatically sign the commit by running the git-commit command with the `-s`
option.  There may be multiple sign-offs if more than one developer was
involved in authoring the contribution.


### Individual vs. Corporate Contributors

Often employers or academic institution have ownership over code that is
written in certain circumstances, so please do due diligence to ensure that
you have the right to submit the code.

If you are a developer who is authorized to contribute to the Dynamic
Simulation Environment on behalf of your employer, then please use your
corporate email address in the Signed-off-by tag.
Otherwise please use a personal email address.


## Maintain Copyright holder / Contributor list

Each contributor is responsible for identifying themselves in the
[NOTICE](NOTICE) file, the project's list of copyright holders and authors.
Please add the respective information corresponding to the Signed-off-by tag
as part of your first pull request.

If you are a developer who is authorized to contribute to the Dynamic Simulation
Environment on behalf of your employer, then add your company / organization to
the list of copyright holders in the [NOTICE](NOTICE) file.
As author of a corporate contribution you can also add your name and corporate
email address as in the Signed-off-by tag.

If your contribution is covered by this project's DCO's clause "(c) The
contribution was provided directly to me by some other person who certified
(a) or (b) and I have not modified it", please add the appropriate copyright
holder(s) to the [NOTICE](NOTICE) file as part of your contribution.

[DCO]: https://developercertificate.org/
[NumPyDoc]: https://sphinxcontrib-napoleon.readthedocs.io/en/latest/example_numpy.html#example-numpy
