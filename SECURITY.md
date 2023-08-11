# Security policy

## Reporting a vulnerability

To report a security problem in Pixie, please contact the Maintainers Team
at <cncf-pixie-maintainers@lists.cncf.io>.

Our team will respond within 3 working days of your email. The team will help
diagnose the severity of the issue and determine how to address the issue.
Issues deemed to be non-critical will be filed as GitHub issues.
Critical issues will receive immediate attention and be fixed as quickly
as possible. This project follows a 90 day disclosure timeline.

## Embargo Policy

This policy forbids members of this project's security contacts and others defined below from sharing information outside of the security contacts and this listing without need-to-know and advance notice.

The information members and others receive from the list defined below must:

- not be made public,
- not be shared,
- not be hinted at
- must be kept confidential and close held
Except with the list's explicit approval. This holds true until the public disclosure date/time that was agreed upon by the list.

If information is inadvertently shared beyond what is allowed by this policy, you are REQUIRED to inform the security contacts of exactly what information leaked and to whom. A retrospective will take place after the leak so we can assess how to not make this mistake in the future.

Violation of this policy will result in the immediate removal and subsequent replacement of you from this list or the Security Contacts.

This individuals on this list are as follows:

- @zasgar
- @vihangm
- @aimichelle

Notification of embargo will follow this [template](https://github.com/cncf/tag-security/blob/main/project-resources/templates/embargo.md).

## Incident response

This serves to define how potential security issues should be triaged, how
confirmation occurs, providing the notification, and issuing a security advisory
as well as patch/release.

### Triage

#### Identify the problem

Triaging problems allows maintainers to focus resources on the most critically
impacting problems. Potential security problems should be evaluated against the
following information:

* Which component(s) of the project is impacted?
* What kind of problem is this?
  * privilege escalation
  * credential access
  * code execution
  * exfiltration
  * lateral movement
* How complex is the problem?
* Is user interaction required?
* What privileges are required for this problem to occur?
  * admin
  * general
* What is the potential impact or consequence of the problem?
* Does an exploit exist?

Any potential problem that has an exploit, permits privilege escalation, is
simple, and does not require user interaction should be evaluated immediately.
[CVSS Version 3.1](https://nvd.nist.gov/vuln-metrics/cvss/v3-calculator) can be
a helpful tool in evaluating the criticality of reported problems.

#### Acknowledge receipt of the problem

Respond to the reporter and notify them you have received the problem and have
begun reviewing it. Remind them of the embargo policy, and provide them
information on who to contact/follow-up with if they have questions. Estimate a
time frame that they can expect to receive an update on the problem. Create a
calendar reminder to contact them again by that date to provide an update.

#### Replicate the problem

Follow the instructions relayed in the problem. If the instructions are
insufficient, contact the reporter and ask for more information.

If the problem cannot be replicated, re-engage the reporter, let them know it
cannot be replicated, and work with them to find a remediation.

If the problem can be replicated, re-evaluate the criticality of the problem, and
begin working on a remediation. Begin a draft security advisory.

Notify the reporter you were able to replicate the problem and have begun working
on a fix. Remind them of the embargo policy. If necessary, notify them of an
extension (only for very complex problems where remediation cannot be issued
within the project's specified window).

##### Request a CVE number

If a CVE has already been provided, be sure to include it on the advisory. If
one has not yet been created, [GitHub functions as a
CNA](https://docs.github.com/en/code-security/security-advisories/about-github-security-advisories#cve-identification-numbers)
and allows you to request one as part of the security advisory process. Provide
all required information and as much optional information as we can. The CVE
number is shown as reserved with no further details until notified it has been
published.

### Notification

Once the problem has been replicated and a remediation is in place, notify
subscribed parties with a security bulletin and the expected publishing date.

### Publish and release

Once a CVE number has been assigned, publish and release the updated
version/patch. Be sure to notify the CVE group when published so the CVE details
are searchable. Be sure to give credit to the reporter by *[editing the security
advisory](https://docs.github.com/en/github/managing-security-vulnerabilities/editing-a-security-advisory#about-credits-for-security-advisories)*
as they took the time to notify and work with you on the problem!

#### Issue a security advisory

Follow the instructions from [GitHub to publish the security advisory previously
drafted](https://docs.github.com/en/github/managing-security-vulnerabilities/publishing-a-security-advisory).

For more information on security advisories, please refer to the [GitHub
Article](https://docs.github.com/en/code-security/security-advisories/about-github-security-advisories).
