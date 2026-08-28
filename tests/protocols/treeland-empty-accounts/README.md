# Empty Accounts startup regression tests

## Scope

- Test source: `tests/protocols/treeland-empty-accounts/`
- Fixtures: one target has the shared AccountsService mock return an empty user
  list; the other deliberately does not register that service.
- Coverage level: **I** (full compositor startup and production `UserModel`
  state).

## Required observable results

Both test runners start the normal Treeland fixture with its headless backend.
Their C Wayland client must connect to the global session socket. It then reads
production state on the compositor thread and asserts that `UserModel` exists,
has zero rows, and has an empty current username.

`test_treeland_empty_accounts` verifies the successful AccountsService reply
whose user list is empty. `test_treeland_accounts_service_unavailable` verifies
that an unavailable AccountsService is handled as a warning rather than a
fatal error.

Both cases retain the current process user's passwd entry. This allows
`SessionManager` to use its `getpwuid()` fallback when the `dde` account does
not exist in the test environment.

## Known boundary

The tests do not make `/etc/passwd` empty. In that condition `getpwuid(getuid())`
also fails, so the compositor cannot establish a global session socket; that
no-crash path requires a separate container or mount-namespace test.
