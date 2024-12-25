**THIS COMES FROM AN INTERNAL DOCUMENT AND MAY BE OUT OF DATE (last updated 2024-10-08)**
# Why have versions at all
If a user finds a bug then we want to know which release of the program they are using so we can recreate it. It may even be fixed in later versions so we may just say "Have you updated the version?".

The version gives the user an idea off how they differ. If major number changed then it's a big change and it can be difficult to upgrade to.

# Version format for BTB
We don't use semantic versioning (for now).

The full format looks like this: `major.minor.patch.revision/name-year.month.day`.
Revision, name and date is optional: `major.minor.patch.[revision]/[name]-[year.month.day]`.

- **major** - Incremented on big changes that are usually incompatibility between versions. (changed maybe 1-2 times a year)
- **minor** - Incremented on medium changes. When change is neither major or patch. (changed every 1-3 months)
- **patch** - Incremented when doing a new quick release. Usually bug fixes. (can be changed every day but more likely every 1-20 days).
- **revision** - Internal number for developers. Ideally, it is incremented on every compilation. (resets when patch,minor,major changes).
- **name** - Any name that describes the release. Can be *hotfix*, *dev*, *test*, *tcp-module*. Letters, numbers and underscore is preferred (try to make it descriptive and neat looking). This is fine: `1.5.6/TCP_upgrade-2024-05-09`.
- **year-month-day** - The date including the full year (2024 not 24) and month/day prefixed with zero (09 not 9).
