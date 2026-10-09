# Claude Code plugin for Konsole

Shows what Claude Code is doing in each Konsole session, modeled on iTerm2's
Claude Code integration: a sessions panel, status dots on tabs, desktop
notifications, optional badges, workgroups (Chat, Diff and Code Review) and
code review clippings. It is driven by iTerm2's own Claude Code hook,
`cc-status`, through `it2` (a stand-in for iTerm2's command of that name), and
by Claude Code's Program Status Protocol reports (OSC 7501).

## Credits and licenses

Most of the plugin is GPL-2.0-or-later, like Konsole. These parts come from
[iTerm2](https://github.com/gnachman/iTerm2) (George Nachman and the iTerm2
authors), which is GPL-2.0, and are GPL-2.0-only:

| File | From iTerm2 |
| --- | --- |
| `sessionstatus.h`, `sessionstatus.cpp` | Ported from `iTermSessionTabStatus.swift`, `iTermTabStatusController.swift`, `SetStatusBuiltInFunction.swift` |
| `programstatusrecords.h`, `programstatusrecords.cpp` | Ported from `iTermProgramStatus.swift` |
| `autotests/SessionStatusTest.cpp`, `autotests/ProgramStatusRecordsTest.cpp` | Ported from iTerm2's tests of the above |
| `workgroup/claudecode-diff` | The waiting message from `DiffWaitingPromptView.swift` |
| `workgroup/review-prompt.txt` | iTerm2's default code review prompt |
| `workgroup/code-review-system-prompt.txt` | iTerm2's `code-review-system-prompt.txt`, first line adapted |
| `workgroup/code-review-settings.json` | Adapted from iTerm2's `code-review-settings.txt` |

Konsole's `src/TabStatus.h` and `src/TabStatus.cpp` (OSC 21337) follow iTerm2's
parser and are GPL-2.0-only for the same reason. `cc-status` itself is not part
of this tree; it is built from iTerm2's source.

The Program Status Protocol is specified at
<https://www.superlogical.com/rex/docs/build/program-status>.
