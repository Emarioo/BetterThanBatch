# Crude version control
A crude version control tool

## Features (incomplete)
Operations on workspace
Snapshot (commits)
Branches

# Implementation
**Workspace** - Your repository. <br>
**Snapshot** - A state of your workspace (commit). Snapshots depend on others.

## Version control files
`.cvc` contains files that keep track of your versions (do not delete it). <br>
`.cvc/config` basic information about the workspace (branches). <br>
`.cvc/snapshots` contains snapshot information and descriptions (commits). <br>
`.cvc/log` a log of all operations. <br>
`.cvc/data` contains actual snapshot data. <br>

## Operations
### Branch
`branch` - list branches <br>
`branch [name]` - switch branch <br>
`branch add [name]` - create branch <br>
`branch del [name]` - delete branch <br>

### Snapshot
`snap` - list snapshots <br>
`snap add` - create new snapshot <br>
`snap desc [name]` - set/replace description of latest snapshot <br>
`snap desc #[id] [name]` - name the latest snapshot <br>
`snap merge #[id_a] #[id_b]` - merge snapshot <br>
`snap del #[id]` - delete snapshot <br>

## Storing file data
Every snapshot has it's own version of all files in workspace.
However, storing data for all files per snapshot is inefficient.
Most files would be duplicated.

**How do we store data efficiently?**
- Some snapshots can share the same file.
- Snapshots can store the change (code lines) in a file instead of the whole file. 


# TODO
- Operations come from the command line. It would be nice if you could do it programmatically too.
- Feed the workspace a list of commands.


# Future
- API for live team collaboration where multiple people can work on the same snapshot where the tool knows who did what.
- Compression optimizations. Data is stored to be easily added and deleted. But it would be neat to have an operation `cvc optimize` which will spend some time optimizing storage.
- A lock so that multiple cvc processes don't mess up the .cvc files. I think git has .lock or something.
