// cod4-live adapter: IW3 Bot Warfare's native interface on the classic engine.
init()
{
    level.bot_builtins[ "printconsole" ] = ::printconsole;
    level.bot_builtins[ "fileexists" ] = ::fileexists;
    level.bot_builtins[ "botaction" ] = ::action;
    level.bot_builtins[ "botstop" ] = ::stop;
    level.bot_builtins[ "botmovement" ] = ::movement;
    level.bot_builtins[ "botmeleeparams" ] = ::melee;
    level.bot_builtins[ "botangles" ] = ::angles;
    level.bot_builtins[ "isbot" ] = ::isbot;
    level.bot_builtins[ "fs_fopen" ] = ::fileopen;
    level.bot_builtins[ "fs_fclose" ] = ::fileclose;
    level.bot_builtins[ "fs_readline" ] = ::readline;
    level thread maps\mp\bots\_bot::init();
}
printconsole( message ) { live_botprint( message ); }
fileexists( path ) { return live_botfileexists( "scriptdata/" + path ); }
action( name ) { self live_botaction( name ); }
stop() { self live_botstop(); }
movement( forward, right ) { self live_botmovement( forward, right ); }
melee( yaw, distance ) { self live_botmelee( yaw, int( distance ) ); }
angles( value ) { self setplayerangles( value ); }
isbot() { return self live_isbot(); }
fileopen( path, mode ) { return live_botfileopen( "scriptdata/" + path, mode ); }
fileclose( handle ) { live_botfileclose( handle ); }
readline( handle ) { return live_botreadline( handle ); }
