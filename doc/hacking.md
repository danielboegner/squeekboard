Hacking
=======

This document describes the standards for modifying and maintaining the *squeekboard* project.

Principles
----------

The project was built upon some guiding principles, which should be respected primarily by the maintainers, but also by contributors to avoid needlessly rejected changes.

The overarching principle of *squeekboard* is to empower users.

Software is primarily meant to solve problems of its users. Often in the quest to make software better, a hard distinction is made between the developer, who becomes the creator, and the user, who takes the role of the consumer, without direct influence on the software they use.
This project aims to give users the power to make the software work for them by blurring the lines between users and developers.

Notwithstanding its current state, *squeekboard* must be structured in a way that provides users a gradual way to gain more experience and power to adjust it. It must be easy, in order of importance:

- to use the software,
- to modify its resources,
- to change its behavior,
- to contribute upstream.

To give an idea of what it means in practice, those are some examples of what has been important for *squeekboard* so far:

- being quick and usable,
- allowing local overrides of resources and config,
- storing resources and config as editable, standard files,
- having complete, up to date documentation of interfaces,
- having an easy process of sending contributions,
- adapting to to user's settings and constrains without overriding them,
- avoiding compiling whenever possible,
- making it easy to build,
- having code that is [simple and obvious](https://www.python.org/dev/peps/pep-0020/),
- having an easy process of testing and accepting contributions.

You may notice that they are ordered roughly from "user-focused" to "maintainer-focused". While good properties are desired, sometimes they conflict, and maintainers should give additional weight to those benefiting the user compared to those benefiting regular contributors.

Sending patches
---------------

By submitting a change to this project, you agree to license it under the [GPL license version 3](https://source.puri.sm/Librem5/squeekboard/blob/master/COPYING), or any later version. You also certify that your contribution fulfills the [Developer's Certificate of Origin 1.1](https://source.puri.sm/Librem5/squeekboard/blob/master/dco.txt).

Development environment
-----------------------

*Squeekboard* is regularly built and tested on [the development environment](https://developer.puri.sm/Librem5/Development_Environment.html).

Recent Fedora releases are likely to be tested as well.

### Dependencies

On a Debian based system run

```sh
sudo apt-get -y install build-essential
sudo apt-get -y build-dep .
```

For an explicit list of dependencies check the `Build-Depends` entry in the [`debian/control`](https://source.puri.sm/Librem5/squeekboard/blob/master/debian/control) file.

Testing
-------

Most common testing is done in CI. Occasionally, and for each release, do perform manual tests to make sure that

- the application draws correctly
- it shows when relevant
- it changes layouts
- it changes views

Testing with an application:

```
python3 tools/entry.py
```

Testing visibility:

```
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b true
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b false
```

Testing layouts:

Layouts can be selected using the GNOME Settings application.

```
# define all available layouts. First one is currently selected.
$ gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us'), ('xkb', 'de')]"
```

Coding
------

### Project structure

Rust modules should be split into 2 categories: libraries, and user interface. They differ in the way they do error handling.

Libraries should:

- not panic due to external surprises, only due to internal inconsistencies
- pass errors and surprises they can't handle to the callers instead
- not silence errors and surprises

User interface modules should:

- try to provide safe values whenever they encounter an error
- do the logging
- give libraries the ability to report errors and surprises (e.g. via giving them loggers)

### Style

Code submitted should roughly match the style of surrounding code. Things that will *not* be accepted are ones that often lead to errors:

- skipping brackets `{}` after every `if()`, `else`, and similar ([SCI CERT C: EXP19-C](https://wiki.sei.cmu.edu/confluence/display/c/EXP19-C.+Use+braces+for+the+body+of+an+if%2C+for%2C+or+while+statement))

Bad example:

```
if (foo)
  bar();
```

Good example:

```
if (foo) {
  bar();
}
```

- mixing tabs and spaces in the same block of code (or config)

Strongly encouraged:

- don't make lines too long. If it's longer than ~80 characters, it's probably unreadable already, and the code needs to be clarified;
- put operators in the beginning of a continuation line

Bad example:

```
foobar = verylongexpression +
    anotherverylongexpression + 
    yetanotherexpression;
```

Good example:

```
foobar = verylongexpression
    + anotherverylongexpression
    + yetanotherexpression;
```

- use `///` for documentation comments in front of definitions and `/*! ... */` for documentation comments in the beginning of modules (see [Rust doc-comments](https://doc.rust-lang.org/reference/comments.html#doc-comments))

If in doubt, check [PEP8](https://github.com/rust-dev-tools/fmt-rfcs/blob/master/guide/guide.md), the [kernel coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html), or the [Rust style guide](https://github.com/rust-dev-tools/fmt-rfcs/blob/master/guide/guide.md).

Maintenance
-----------

Squeekboard uses Rust & Cargo for some of its dependencies.

Use the `cargo.sh` script for maintaining the Cargo part of the build. The script takes the usual Cargo commands, after the first 2 positional arguments: source directory, and output artifact. So, `cargo test` becomes:

```
cd build_dir
sh /source_path/cargo.sh test
```

### Cargo dependencies

All Cargo dependencies must be selected in the version available in PureOS, and added to the file `debian/control`. Please check with https://software.pureos.net/search_pkg?term=librust .

Dependencies must be specified in `Cargo.toml` with 2 numbers: "major.minor". Since bugfix version number is meant to not affect the interface, this allows for safe updates.

`Cargo.lock` is used for remembering the revisions of all Rust dependencies. It must correspond to the default dependency configuration: without flags to use older or newer versions of dependencies. It should be updated often, preferably with each bugfix revision, and in a commit on its own:

```
cd build_dir
ninja ./Cargo.toml
sh /source_path/cargo.sh update
ninja test
cp ./Cargo.lock /source_path/
```

Since version 1.9.3, `Cargo.lock` is not actually used by the build system, due to `Cargo.toml` being generated at every build.
