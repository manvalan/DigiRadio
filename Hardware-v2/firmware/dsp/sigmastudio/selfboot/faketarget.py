"""Finto target TCP per SigmaStudio+: registra le scritture e risponde alle letture. Solo locale, nessuna scheda."""
import socket,sys,struct,time,binascii,json
log=open(sys.argv[1]+'.log','a',buffering=1)
rec=open(sys.argv[1]+'.frames','a',buffering=1)
mem={0xF405:[0,1],0xF004:[0,1]}   # CORE_STATUS running, PLL_LOCK locked
s=socket.socket();s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);s.bind(('127.0.0.1',8086));s.listen(5)
log.write('listening\n')
OK=int(sys.argv[2]) if len(sys.argv)>2 else 0
while True:
    c,a=s.accept();log.write(f'connect {a}\n')
    buf=b''
    try:
        while True:
            d=c.recv(1<<20)
            if not d: log.write('closed\n');break
            buf+=d
            while buf:
                k=buf[0]
                if k==0x1b:
                    if len(buf)<11: break
                    log.write('hello '+buf[:11].hex()+'\n');buf=buf[11:]
                elif k==0x09:
                    if len(buf)<14: break
                    tot=struct.unpack('>I',buf[3:7])[0]
                    if len(buf)<tot: break
                    ln=struct.unpack('>I',buf[8:12])[0];ad=struct.unpack('>H',buf[12:14])[0]
                    data=buf[14:14+ln]
                    rec.write(json.dumps({'t':'w','sl':buf[1],'ch':buf[2],'chip':buf[7],'a':ad,'d':data.hex()})+'\n')
                    log.write(f'W a={ad:#06x} n={ln} chip={buf[7]} ch={buf[2]} {data[:10].hex()}\n')
                    buf=buf[tot:]
                elif k==0x0A:
                    if len(buf)<14: break
                    tot=struct.unpack('>I',buf[1:5])[0]
                    if len(buf)<tot: break
                    chip=buf[5];ln=struct.unpack('>I',buf[6:10])[0];ad=struct.unpack('>H',buf[10:12])[0]
                    v=bytes(mem.get(ad,[0]*ln))[:ln].ljust(ln,b'\0')
                    log.write(f'R a={ad:#06x} n={ln} chip={chip}\n')
                    rec.write(json.dumps({'t':'r','a':ad,'n':ln,'chip':chip})+'\n')
                    c.sendall(bytes([0x0B])+struct.pack('>I',14+ln)+bytes([chip])+struct.pack('>I',ln)+struct.pack('>H',ad)+bytes([OK,0])+v)
                    buf=buf[tot:]
                else:
                    log.write('unk '+buf[:40].hex()+'\n');buf=b''
    except Exception as e: log.write(f'err {e}\n')
    c.close()
