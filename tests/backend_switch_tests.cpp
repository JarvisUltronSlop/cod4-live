#include <live/backend_switch.h>
#include <cstdio>
#include <cstdlib>
#include <limits>

void Require(bool ok,const char* message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1); }
}
int main() {
    using live::controller::BackendSwitch;
    using Result=BackendSwitch::Result;
    BackendSwitch trial;
    int saved=0;
    Require(trial.Effective(saved)==0,"staged UI choices never switch the provider");
    Require(!trial.Begin(saved,0,10)&&!trial.Begin(saved,2,10),"unchanged/invalid choices do not start discovery");
    Require(trial.Begin(saved,1,100),"confirmation starts a trial");
    Require(trial.Effective(saved)==1&&saved==0,"trial selects a provider without changing the saved setting");
    Require(!trial.Begin(saved,0,101),"a second apply cannot replace the active trial");
    Require(trial.Observe(saved,false,15099)==Result::None,"missing controller has a full 15-second window");
    Require(trial.Observe(saved,false,15100)==Result::Revert&&trial.Effective(saved)==0,"timeout restores working provider");
    Require(trial.Observe(saved,true,15101)==Result::None,"late samples cannot commit a failed trial");
    Require(trial.Begin(saved,1,16000),"retry is allowed");
    Require(trial.Observe(saved,true,16001)==Result::Commit,"selected provider must report a controller before saving");
    saved=trial.target;
    Require(trial.Effective(saved)==1&&!trial.pending,"successful trial stays on the new saved provider");
    Require(trial.Begin(saved,0,17000),"switching back is supported");
    saved=0;
    Require(trial.Observe(saved,true,17001)==Result::Revert&&trial.Effective(saved)==0,"external config change cancels stale confirmation");
    Require(trial.Begin(saved,1,18000),"cancel fixture");
    trial.pending=false;
    Require(trial.Effective(saved)==0&&trial.Observe(saved,true,18001)==Result::None,"cancel/shutdown never saves an unverified provider");
    const auto start=std::numeric_limits<unsigned>::max()-1000;
    Require(trial.Begin(saved,1,start),"wrapped clock fixture");
    Require(trial.Observe(saved,false,start+14999u)==Result::None,"wrap does not shorten timeout");
    Require(trial.Observe(saved,false,start+15000u)==Result::Revert,"timeout works across clock wrap");
    std::puts("Backend confirmation and recovery checks passed");
}
