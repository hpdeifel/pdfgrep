If you want to contribute to pdfgrep, this file is for you!

* Contributing

You don't have to write code to help pdfgrep. Testing, reporting bugs,
writing/fixing documentation or packaging it for distributions is just
as important. If you want to help, just drop me [1] a mail.

For those who rather get their hands dirty with code, please read the
following sections:

* Coding Style

The pdfgrep source is formatted in the classical Linux-kernel style
described in Documentation/CodingStyle in the kernel's source tree.

In particular this means that we use tabs for indentation.

The code is written in a very Cish C++, that is: Use C++ where we need
it to access the poppler API and C otherwise. The reason for this is
that I feel that most of C++'s features would increase the complexity
of such a small project like pdfgrep and make it difficult to use
standard Posix functions (e.g. std::string and regcomp/regexec).

BUT: These are just guidelines. If a C++ feature makes sense to you,
use it! If a (minor) violation of the indentation rules makes the code
more readable: do it!

* Sending Patches

You can either send your patches directly to me [1], to the mailing-list [2] or
create a pull request on GitLab [3].


Keep on hacking!

[1] mailto:hpd@hpdeifel.de
[2] mailto:pdfgrep-users@pdfgrep.org
[3] https://gitlab.com/pdfgrep/pdfgrep
