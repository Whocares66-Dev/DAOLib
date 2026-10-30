// The options screen's own controls, recorded as the movie makes them (dao/Options.h, DAOLib #273).
//
// tools/options_record.py compiles this with FFDec into src/dao/generated/OptionsRecord.h, and
// dao/Options.cpp appends it to optionsmenu.gfx's first frame each time the game loads the movie.
// It runs there before the game starts the screen's scene (StartSceneFromGame, which the engine
// calls after the frame's script), and so before the scene's set-up
// (OptionsMenu.SetupOptionsMenuInitFunctions) asks OptionsUtils' helpers for each control's set-up
// function. Each helper is wrapped: the set-up function it returns keeps on its control's clip
// which helper made it and with what (the option's name, its label, its range or values), and
// lists the clip. So the movie stays the one source of what a control is.
//
// Every copy of the library a plugin links appends this, so it is written to run more than once:
// the lists are made once and a helper is wrapped once.

if (_global.DAOLibOptionControls == undefined)
{
    _global.DAOLibOptionControls = new Array();
}

_global.DAOLibWrapOptionMaker = function(maker, made)
{
    var wrapper = function()
    {
        var madeWith = arguments;
        var init = made.apply(OptionsUtils, arguments);
        return function()
        {
            this.DAOLibMaker = maker;
            this.DAOLibMadeWith = madeWith;
            _global.DAOLibOptionControls.push(this);
            return init.apply(this, arguments);
        };
    };
    wrapper.DAOLibWrapped = true;
    return wrapper;
};

var DAOLibMakers = new Array("MakeOptionToggleInit", "MakeSliderInit", "MakeSliderPickerInit", "MakePickerInit",
                             "MakeSliderPickerInitFromListAttribute", "MakePickerInitFromListAttribute");
var DAOLibMakerAt = 0;
while (DAOLibMakerAt < DAOLibMakers.length)
{
    var DAOLibMaker = DAOLibMakers[DAOLibMakerAt];
    if (OptionsUtils[DAOLibMaker] != undefined && OptionsUtils[DAOLibMaker].DAOLibWrapped != true)
    {
        OptionsUtils[DAOLibMaker] = _global.DAOLibWrapOptionMaker(DAOLibMaker, OptionsUtils[DAOLibMaker]);
    }
    DAOLibMakerAt = DAOLibMakerAt + 1;
}

// Whether a control that follows a checkbox's state (a volume after its sound's) is taken: as
// OptionsUtils.EnableDisableSliderPicker has it.
_global.DAOLibTiedOn = function(tiedTo, state)
{
    if (tiedTo == undefined || tiedTo == null)
    {
        return true;
    }
    return Number(ExternalCommands.GetValue("ClientOptions." + tiedTo)) == Number(state);
};

// The game's controls the screen shows now, a line each, its fields split by tabs: the clip's
// path, the helper, the option, the label (a string's number or its text), the value, 1 if it
// takes a change, then a slider's least and most, or a choice's values' labels split by
// character 1. The clips of a page no longer shown are dropped from the list.
_global.DAOLibOptionList = function()
{
    var sep = String.fromCharCode(1);
    var lines = new Array();
    var kept = new Array();
    var all = _global.DAOLibOptionControls;
    for (var i = 0; i < all.length; i++)
    {
        var c = all[i];
        if (c._target == undefined || c._parent == undefined)
        {
            continue;
        }
        kept.push(c);
        var a = c.DAOLibMadeWith;
        var maker = c.DAOLibMaker;
        var option = a[0];
        var value = ExternalCommands.GetValue("ClientOptions." + option);
        var on = true;
        var extra = "";
        if (maker == "MakeSliderInit")
        {
            on = _global.DAOLibTiedOn(a[5], a[6]);
            extra = a[3] + "\t" + a[4];
        }
        else if (maker == "MakeSliderPickerInit")
        {
            on = _global.DAOLibTiedOn(a[4], a[5]);
            extra = a[3].join(sep);
        }
        else if (maker == "MakePickerInit")
        {
            extra = a[3].join(sep);
        }
        else if (maker == "MakeSliderPickerInitFromListAttribute")
        {
            on = ExternalCommands.GetNumMembers("ClientOptions." + a[3]) >= 2;
            extra = c.aValueLabels.join(sep);
        }
        else if (maker == "MakePickerInitFromListAttribute")
        {
            var names = new Array();
            var count = ExternalCommands.GetNumMembers("ClientOptions." + a[3]);
            for (var n = 0; n < count; n++)
            {
                names.push(ExternalCommands.GetValue("ClientOptions." + a[3] + "." + n));
            }
            extra = names.join(sep);
        }
        lines.push(c._target + "\t" + maker + "\t" + option + "\t" + a[1] + "\t" + value + "\t" + (on ? "1" : "0") + "\t" + extra);
    }
    _global.DAOLibOptionControls = kept;
    return lines.join("\n");
};
