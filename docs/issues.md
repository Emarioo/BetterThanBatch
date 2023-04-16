# Random issues I decided to write about

## Double BC_DEL with while and break
```
while 1 {
    break
}
```
Parsing the code above results in the instructions below.
!()[img/issue-while-break.png]

The image shows the flow of the bytecode in orange.
The blue and red represents the creation and deletion
of value in register 11.

The issue is that BC_DEL may be called twice on $11.
This happens when we follow the middle path until the jump
in the middle. The second BC_DEL $11 (red) is wasted
on deleting nothing. The parser isn't made to
handle sudden jumps out of the while body.
Hence, the second delete.

!()[img/issue-while-break-2.png]

The solution is rather simple (see above image).
Since the jump out of the while body is the issue
we can make it skip the second BC_DEL. We also need to
rearrange BC_DEL $10 because we always need to execute it.

Now, we will never encounter two deletes whichever route we go.

(me babbling about stuff)
Was this issue really a big deal? Not really but
we can assume that we may have many while loops and breaks
and in a larger program or one that runs for a while we
would end up wasting many instructions. Still, that might be fine,
computers are fast. The problem is that this mindset will
result in wasted instructions for each new feature we don't
properly fix. The whole language would be slow and inefficient.
So fixing issues like this is a good thing.