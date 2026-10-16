# Discord release notifications

The GitHub Actions workflow in `.github/workflows/discord-release-notification.yml`
posts an embed when a release is published in this repository.

## Configure the webhook secret

In the **new repository**, open **Settings → Secrets and variables → Actions**
and create a repository secret named `DISCORD_REPO_UPDATE`. Set its value to the
Discord incoming webhook URL itself; the secret name is not the URL.

Never share the webhook URL in chat, add it to workflow files, or commit it to
the repository. The workflow reads it only from the Actions secret and does not
print it.

## Test

Create a test release in the new repository and publish it. A draft does not
send a notification; publishing the release does. The workflow intentionally
has no manual-dispatch trigger, and rerunning a completed workflow run does not
send a second notification.
