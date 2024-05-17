# Contributing tips for Newcomers

Like most projects, gnome-shell has a few rules and conventions around the coding style and the usage of git. When you make your first MR to gnome-shell, here's a few basics to get started.

## Look at MRs that were merged already

In general it's a good idea to look at a few [merged MRs](https://gitlab.gnome.org/GNOME/gnome-shell/-/merge_requests?scope=all&state=merged) before making your first MR. This will help you get a good idea of what commit messages and code changes usually look like. 

## JavaScript coding style

The gnome-shell codebase has fairly consistent coding style. When working on gnome-shell, you should be on the safe side by following the coding style of the existing code around you.

The full JS coding style rules are [here](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/main/docs/js-coding-style.md).

## Writing commit messages

Commit messages should always have summary and a description. The first line (summary) of your commit message should quickly tell what change was made, and the description should explain why that change was made (so that people looking at the git history later don't have to read through Gitlab to understand why a change was made).

[Here's an example and more tips](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/main/docs/commit-messages.md#example) for a good commit message. The [GNOME Handbook](https://handbook.gnome.org/development/commit-messages.html) also has a good section on commit messages. And of course just can always read a bit through the git history.

## Keep the commit history clean

gnome-shell tries to keep a clean git history (no merge commits or "apply review" commits).

When you propose changes, try to group them into a "clean" series of individual commits, where each commit is changing a single thing and explains why that is done.

For larger changes, the commit history should be almost similar to telling a story. Ideally, the reviewer can read through your "story" of commit messages from beginning to end, and can understand what you're changing without having to look at the code at all.

## Applying and responding to reviews

When applying changes requested by reviews, use `git rebase -i` and make changes to your individual commits (`git commit --amend`, you might want to learn about fixup commits too), then force-push the updated branch to your MR.

If you use Gitlab's "Apply Suggestion" feature, that will add an extra commit to your branch instead of updating the actual commit where you're making the change, so you'll have to squash that extra commit into the actual commit manually.

## Gitlab MR title and description

If the MR has just a single commit, you can just use the commit title and description for the MR too (GitLab does this by default).

If the MR has multiple commits, write a short overview of what you're changing (doesn't have to be long, the full reasoning and explanations should always be in the commit messages).


