"""A reconnect must expose its nonce before the next 100ms status publication."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'main/hr_management.c').read_text()
def function(name):
 at=s.index('void '+name+'(');end=s.index('{',at)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[at:end]
fixture='''
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#define HR_MANAGEMENT_BYTES 228
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static int mux;static uint8_t snapshot[228];static atomic_uint nonce;
static uint32_t random_value=42;
static uint32_t esp_random(void){return random_value;}
static void put32(uint8_t*p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=x>>(i*8);}
'''
main='''int main(void){uint8_t b[228];memcpy(snapshot,"HBC1",4);hr_management_connection(true);hr_management_read(b);assert(b[216]==42);random_value=77;hr_management_connection(true);hr_management_read(b);assert(b[216]==77);hr_management_connection(false);hr_management_read(b);assert(!b[216]);assert(!memcmp(b,"HBC1",4));}'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(fixture+function('hr_management_connection')+function('hr_management_read')+main);exe=Path(td)/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra',str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
