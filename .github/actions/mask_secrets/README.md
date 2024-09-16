# Mask Secrets Github Action

Iterate all GitHub secrets and replace occurrences treewide with
`***NAME_OF_SECRET***`.

This is intended for use before the `upload-artifact` action to mask
secrets found in log files as a result of the CI tests. This prevents
secrets from appearing in publicly-downloadable archives attached to
workflow runs.

## Known Issues

1. Replacements occur tree-wide. This should be used after the tests
   have run to avoid corrupting tests.
2. The regex used for the replacement cannot be applied to secrets that
   have linefeed characters in them. These secrets will be skipped
   without notice.

## Usage

```
  - name: Mask secrets in logs
    id: mask-logs
    if: always()
    uses: ./.github/actions/mask_secrets
    with:
      secrets-json: ${{ toJson(secrets) }}

  - name: Upload artifacts
    uses: actions/upload-artifact@v4
    if: always() && steps.mask-logs.outcome == 'success'
    with:
      name: name-for-the-uploaded-archive
      path: |
        path-to/file-to-archive.log
```

- The `uses` path may change based on how your workflow checks out the
  repository. (eg: `uses:
  ./modules/lib/golioth-firmware-sdk/.github/actions/mask_secrets`).
- Secrets must be passed as serialized JSON as in the example above.
  This is because actions cannot inherit secrets. Reusable workflows can
  inherit secrets but they cannot be run as steps (only as jobs).
- Use the `if` step cited above to ensure that if the mask secrets step
  fails, no artifacts are uploaded.
