Language syntax:

// comment

/* comment */

def const = expr ;

def function-name ()
{
  /* statements */
}

def function-name ( arg [ , arg ]... )
{
  /* statements */
}

return;

return expr;

var-statement:

    var variable-def [ , variable-def ]... ;
    
variable-def:

    variable [ scalar-initializer ]
    variable '[' size ']' [ array-initializer ]
    
scalar-initializer:

    = constant-expr
    
array-initializer:

    = { constant-expr [ , constant-expr ]... }

if ( expr ) statement

if ( expr ) statement else statement

for ( init-expr; test-expr; inc-expr ) statement

while ( test-expr ) statement

do statement while ( test-expr )

continue ;

break ;

label:

goto label ;

print expr [ $|, expr ]... [ $ ] ;

expr = expr

expr += expr
expr -= expr
expr *= expr
expr /= expr
expr %= expr
expr &= expr
expr |= expr
expr ^= expr
expr <<= expr
expr >>= expr

expr && expr
expr || expr

expr ^ expr
expr | expr
expr & expr

expr == expr
expr !=  expr

expr < expr
expr <= expr
expr >= expr
expr > expr

expr << expr
expr >> expr

expr + expr
expr - expr
expr * expr
expr / expr
expr % expr

- expr
~ expr
! expr
++expr
--expr
expr++
expr--

function()
function ( arg [, arg ]... )
array [ index ]

(expr)
var
integer
"string"

Built-in variables:

clkfreq
par
cnt
ina
inb
outa
outb
dira
dirb
ctra
ctrb
frqa
frqb
phsa
phsb
vcfg
vscl

Built-in Functions:

waitcnt(target)
waitpeq(state, mask)
waitpne(state, mask)


