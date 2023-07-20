# SPDX-License-Identifier: Apache-2.0

import re

from gitlint.rules import CommitRule, RuleViolation


class SignedOffBy(CommitRule):
    """ This rule will enforce that each commit contains a "Signed-off-by" line.
    We keep things simple here and just check whether the commit body contains a line that starts with "Signed-off-by".
    """

    # A rule MUST have a human friendly name
    name = "body-requires-signed-off-by"

    # A rule MUST have an *unique* id, we recommend starting with UC (for User-defined Commit-rule).
    id = "UC1"

    def validate(self, commit):
        flags = re.UNICODE
        flags |= re.IGNORECASE
        for line in commit.message.body:
            if line.lower().startswith("signed-off-by"):
                if not re.search(r"(^)Signed-off-by: ([-'\w.]+) ([-'\w.]+) (.*)", line, flags=flags):
                    return [RuleViolation(self.id, "Signed-off-by: must have a full name", line_nr=1)]
                else:
                    return
        return [RuleViolation(self.id, "Body does not contain a 'Signed-off-by:' line", line_nr=1)]
