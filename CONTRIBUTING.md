# Contributing to Golioth's Firmware SDK

First off, thanks for taking the time to contribute! ❤️

Contributions of all types are encouraged and valued.
We promise to work towards automating as much of the contribution process
as possible but request you refer to the sections below before contributing. It will make it a
lot easier for us maintainers and smooth out the experience for all involved. The community looks
forward to your contributions. 🎉

> And if you like the project, but just don't have time to contribute, that's fine.
> There are other easy ways to support the project and show your appreciation,
> which we would also be very happy about:
> - Star the project
> - Tweet about it & mention [@golioth_iot](https://twitter.com/golioth_iot)
> - Refer this project in your project's readme
> - Mention the project at local meetups and tell your friends/colleagues

## Code of Conduct

This project and everyone participating in it is governed by the
[Golioth Firmware SDK Code of Conduct](https://github.com/golioth/golioth-firmware-sdk/blob/main/CODE_OF_CONDUCT.md).
By participating, you are expected to uphold this code. Please report unacceptable behavior
to <coc@golioth.io>.

## I Have a Question

After referring to our [Documentation](https://docs.golioth.io) and searching for existing
[Issues](https://github.com/golioth/golioth-firmware-sdk/issues), feel free to ask a question or
start a discussion in our [Forum](https://forum.golioth.io/).

- Provide as much context as you can about what you're running into.
- Provide project and platform versions (target board, version of our SDK), depending on what seems relevant.

We will then answer your question as soon as possible.

## I Want To Contribute

> ### Legal Notice
> When contributing to this project, you must agree that you have authored 100% of the content,
> that you have the necessary rights to the content and that the content you contribute may be
> provided under the project license.

### Developer Certificate of Origin Process

This project will only accept contributions using the Developer’s Certificate of Origin 1.1 located
at https://developercertificate.org (“DCO”). The DCO is a legally binding statement asserting that
you are the creator of your contribution, or that you otherwise have the authority to distribute
the contribution, and that you are intentionally making the contribution available under the
license associated with the Project ("License").

You can agree to the DCO in your contribution by using a “Signed-off-by” line at the end of your
commit message. You should only submit a contribution if you are willing to agree to the DCO terms.
If you are willing, just add a line to the end of every git commit message:

```
Signed-off-by: Jane Smith <jane.smith@email.com>
```

You may type this line on your own when writing your commit messages. However, Git makes it easy
to add this line to your commit messages. If you set your user.name and user.email as part of your
git configuration, you can sign your commit automatically with `git commit -s`.

## Reporting Bugs

### Before Submitting a Bug Report

We have a special place in our hearts for embedded developers and have been on the receiving
end of some nasty silicon errata and software bugs. Because of this we genuinely appreciate the
time and effort you put into uncovering bugs not only in our software but yours as well.

To best help you out we *recommend* that you gather the following information before making a bug report.

- Make sure that you are using the latest supported version
- Check if there is not already a bug report existing for your bug or error in the [bug tracker](https://github.com/golioth/golioth-firmware-sdk/issues?q=label%3Abug).
- Collect information about the bug:
  - Stack trace
  - OS, Platform and Version (Windows, Linux, macOS, x86, ARM)
  - Version of the interpreter, compiler, SDK, runtime environment, package manager, depending on what seems relevant.
  - Possibly your input and the output
  - Can you reliably reproduce the issue? And can you also reproduce it with older versions?
- Determine if your bug is really a bug and not an error on your side e.g. using incompatible
  environment components/versions (Make sure that you have read the [documentation](https://docs.golioth.io).
  If you are looking for support, you might want to check [this section](#i-have-a-question)).

However we also realize there are those times where you just need a good ol' sanity check.
If it's late and you find yourself debugging assembly code please feel free to reach out to us
on [Discord](https://discord.com/invite/qKjmvzMVYR) for some re-assurance.


### How Do I Submit a Good Bug Report?

> We take security very seriously. It is *vitally* important to NOT publicly report
> security related issues, vulnerabilities or bugs. Instead, please send your findings
> by email to <security@golioth.io>. We appreciate your research and will work with you
> to resolve the situation ASAP.
<!-- You may add a PGP key to allow the messages to be sent encrypted as well. -->

Upon following the recommendations in the previous section:

- Open a [Bug Report](https://github.com/golioth/golioth-firmware-sdk/issues/new/choose)
- Complete the Bug Report form
- Include any information gathered in the previous section
- Please provide as much context as possible and describe the *reproduction steps* that someone
  else can follow to recreate the issue on their own.
- For good bug reports isolate the problem and create a reduced test case.

Once it's filed:

- The project team will label the issue accordingly.
- A team member will try to reproduce the issue with your provided steps. If there are no
  reproduction steps or no obvious way to reproduce the issue, the team will ask you for
  those steps and mark the issue as `needs-repro`. Bugs with the `needs-repro` tag will not be
  addressed until they are reproduced.
- If the team is able to reproduce the issue, it will be marked `needs-fix`, as well as possibly
  other tags (such as `critical`), and the issue will be left to be [implemented by someone](#your-first-code-contribution).

<!-- You might want to create an issue template for bugs and errors that can be used as a guide and that defines the structure of the information to be included. If you do so, reference it here in the description. -->

## Suggesting Enhancements

This section guides you through submitting an enhancement suggestion for Golioth's Firmware SDK,
**including completely new features and minor improvements to existing functionality**.
Following these guidelines will help maintainers and the community to understand your suggestion
and find related suggestions.

### Before Submitting an Enhancement

- Make sure that you are using the latest version.
- Read the [documentation](https://docs.golioth.io) carefully and find out if the functionality is
  already covered, maybe by an individual configuration.
- Perform a [search](https://github.com/golioth/golioth-firmware-sdk/issues) to see if the
  enhancement has already been suggested. If it has, add a comment to the existing issue instead
  of opening a new one.
- Find out whether your idea fits with the scope and aims of the project. It's up to you to make a
  strong case to convince the project's developers of the merits of this feature. Keep in mind
  that we want features that will be useful to the majority of our users and not just a small subset.
  If you're just targeting a minority of users, consider writing an add-on/plugin library.

### How Do I Submit a Good Enhancement Suggestion?

Enhancement suggestions are currently tracked as discussions in our
[Forum](https://forum.golioth.io/). We recommend establishing the following in your discussion topic
and description:

- Use a **clear and descriptive title** for the issue to identify the suggestion.
- Provide a **step-by-step description of the suggested enhancement** with as many details as possible.
- **Describe the current behavior** and **explain which behavior you expected to see instead** and
  why. At this point you can also tell which alternatives do not work for you.
- You may want to **include screenshots and animated GIFs** which help you demonstrate the steps
  or point out the part which the suggestion is related to. You can use
  [this tool](https://www.cockos.com/licecap/) to record GIFs on macOS and Windows, and
  [this tool](https://github.com/colinkeenan/silentcast) or
  [this tool](https://github.com/GNOME/byzanz) on Linux. <!-- this should only be included if the project has a GUI -->
- **Explain why this enhancement would be useful** to most SDK users. You may also want to point
  out the other projects that solved it better and which could serve as inspiration.

<!-- You might want to create an issue template for enhancement suggestions that can be used as a guide and that defines the structure of the information to be included. If you do so, reference it here in the description. -->

## Style Guide

Follow the guidelines in our [Style Guide](docs/Style_Guide.md).

## Commit Guide

Each commit message should contain:

* a short and descriptive subject line followed by a blank line, which includes a
  prefix that identifies the subsystem being changed, followed by a colon.
* a body which explains the reason for the change ("why" the code is changed).
* a `Signed-off-by: <name> <email>` line (automatic with `git commit -s`).

Here's an example:

```
golioth_rpc: add void *callback_arg

Added a convenience callback_arg, so user can capture their own
data and forward it to the RPC method callback when it's invoked.

Signed-off-by: Nick Miller <nick@golioth.io>
```

If the change addresses an issue, include a line of the form:

```
Fixes #<issuenumber>
```

## Improving The Documentation

- If you want to assist in improving comments in line with code in our sources feel free to make an issue or PR
- If improving handbooks or guides are your interest head on over to [github.com/golioth/docs](https://github.com/golioth/docs), which contains the sources for our formal documentation hosted at [docs.golioth.io](https://docs.golioth.io/)

    ----
### This guide is based on the **contributing-gen**. [Make your own](https://github.com/bttger/contributing-gen)!
