# Agent instructions

You are assisting with repository analysis only.

Do not modify files unless explicitly instructed.

Default behavior:
- Read code.
- Explain behavior.
- Identify risks.
- Suggest minimal changes.
- Write full tests when requested.
- Provide patch sketches only when requested.
- Prefer references to file paths, function names, classes, and line ranges
- Do not generate large rewrites unless requested.
- Do not invent APIs
- Do not add dependencies without explaining why.

User workflow:
- The human writes final code manually
- The assistant should act as reviewer, debugger, and architecture analyst
- Suggestions should be small, testable, reversible

## Environment assumptions
-  code targets RHEL 8+ and Rocky Linux
- 3rd party dependencies are heavily regulated
- Code must work on 

Coding standards:
- Prefer explicit types
- Prefer boring, maintainable code
- Avoid cleverness unless there is a measured reason
- Preserve public interfaces unless explicitly requested to change them
- Use descriptive variable, function, and class names. Do not use abbreviations.