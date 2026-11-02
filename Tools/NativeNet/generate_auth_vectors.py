import hashlib
import hmac
from pathlib import Path
H=lambda b:hashlib.sha1(b).digest()
L=lambda n:n.to_bytes(32,'little')
N=int('894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7',16)
g=7;salt=bytes(range(32));b=0x123456789abcdef0123456789abcdef
x=int.from_bytes(H(salt+H(b'FIXTURE:AUTHORED-PASSWORD')),'little');v=pow(g,x,N);B=(3*v+pow(g,b,N))%N
seed=bytes(range(16));state=H(seed);buf=H(state+bytes(20)+state);buf2=H(state+buf+state);a=int.from_bytes(buf+buf2[:12],'little')+256;A=pow(g,a,N);u=int.from_bytes(H(L(A)+L(B)),'little');S=pow((B-3*v)%N,a+u*x,N)
shared=L(S).lstrip(b'\0');shared=shared[len(shared)%2:];even=H(shared[::2]);odd=H(shared[1::2]);K=bytes(sum(zip(even,odd),()))
M=H(bytes(a^b for a,b in zip(H(L(N)),H(bytes([g]))))+H(b'FIXTURE')+salt+L(A)+L(B)+K);M2=H(L(A)+M+K)
vals={'Prime':L(N),'Salt':salt,'ServerPrivate':L(b),'Verifier':L(v),'ServerPublic':L(B),'ExpectedA':L(A),'ExpectedK':K,'ExpectedM':M,'ExpectedM2':M2}
vals['ExpectedVersionChecksum']=hmac.digest(bytes(range(0xa0,0xb0)),b'authored-exe','sha1')
world_prefix=b'FIXTURE'+b''.join(i.to_bytes(4,'little') for i in range(8))
world_nonce=0
while H(world_prefix+world_nonce.to_bytes(8,'little'))[0]:world_nonce+=1
vals['ExpectedWorldWork']=world_nonce.to_bytes(8,'little')
vals['ExpectedWorldDigest']=H(b'FIXTURE'+bytes(4)+(0x11223344).to_bytes(4,'little')+(0x12345678).to_bytes(4,'little')+K)
text='// Authored SRP fixture vectors generated with Python hashlib/pow; no account or game data.\n'
for key,val in vals.items():text+=f'const auto {key} = Hex("{val.hex()}");\n'
Path('Tools/NativeNet/auth_vectors.h').write_text(text)
