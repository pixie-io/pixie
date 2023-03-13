# Inclusive Style Guide

We write our code, documentation, review comments, and other assets with inclusivity and diversity in mind. This page is not an exhaustive reference but describes some general guidelines and examples that illustrate some best practices to follow. These are only suggestions. The author, reviewers, and project maintainers will make the final call.

## Avoid ableist language

When trying to achieve a friendly and conversational tone, problematic ableist language may slip in. This can come in the form of figures of speech and other turns of phrase. Be sensitive to your word choice, especially when aiming for an informal tone. Choose alternative words depending on the context.

Things to avoid:

- Phrases referring to mental health: sanity / crazy / insane
- Words related to physical or mental disabilities: blind / cripple / dummy

How to fix ableist language:

- Try replacing the word with a phrase that has no underlying negative connotations. Example: “crazy outliers” → “baffling outliers”, “dummy variable” → “placeholder variable”.

## Use gender-neutral language

Some points in our code, documentation and comments contain needless assumptions about the gender of a future reader, user, etc. Example: “When the user logs into his profile.”

Things to avoid:

- Gendered pronouns: he / she / him / her / his / hers, etc.
- Instances of the phrases “he or she”, “his/hers”, “(s)he”, etc. All of these still exclude those who don't identify with either gender, and implicitly (slightly) favor one gender via listing it first.
- “Guys” as a gender-neutral term, which has male associations. Usually in comments it implies anthropomorphism of inanimate objects and should be replaced with a more precise technical term. If it does refer to people, consider using “everyone”, “folks”, “people”, “peeps”, “y'all”, etc.
- Other gendered words: “brother”, “mother”, “man”, etc.

Cases that are likely fine to leave alone include:

- References to a specific person (“Rachel is on leave; update this when she is back.”).
- A name (“Guy” and “He” are both valid names).
- A language code (“he” is the ISO 639-1 language code for Hebrew).
- He as an abbreviation for “helium”.
- The Spanish word “he”.
- References to a specific fictional person (Alice, Bob, ...).
- For new code/comments, consider using just ‘A’, ‘B’ as names.
- Quotations and content of things like public-domain books.
- Partner agreements and legal documents we can no longer edit.
- Occurrences in randomly generated strings or base-64 encodings.
- Content in a language other than English unless you are fluent in that language.

How to fix gendered language:

- Try rewording things to not involve a pronoun at all. In many cases this makes the documentation clearer. Example: “I tell him when I am all done.” → “I tell the owner when I am all done.” This saves the reader a tiny bit of mental pointer-dereferencing.
- Try using singular they.
- Try making hypothetical people plural. “When the user is done he'll probably...” → “When users complete this step, they probably...”.
- When referring to a non-person, “it” or “one” may be good alternatives (wikipedia link).

## Use racially-neutral language

Some phrases have underlying racial connotations, or reinforce the notion that some races are better than others.

Things to avoid:

- Terms that reinforce the notion that one race is good or bad: blacklist, whitelist
- Words rooted in historical events regarding race: slave, master

How to fix racist language:

- Replace the term with an alternative that does not change its meaning. Example: “whitelist” →  “allowlist”, “master” → “main”.

## References

- <https://developers.google.com/style/inclusive-documentation>
- <https://chromium.googlesource.com/chromium/src/+/master/styleguide/inclusive_code.md>
