#pragma once

#include "Sf2Info.h"

// ===========================================================================
//  The SoundFont library: a list of SF2 files on the user's disk (they are
//  referenced, never copied). Stored in %APPDATA%/Pepperwood/Cohesion LA Font.
// ===========================================================================
namespace fontlib
{
    inline juce::File userDir()
    {
        const auto overrideDir = juce::SystemStats::getEnvironmentVariable ("COHESION_LAFONT_DIR", {});
        if (overrideDir.isNotEmpty()) return juce::File (overrideDir);
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("Pepperwood").getChildFile ("Cohesion LA Font");
    }
    inline juce::File listFile() { return userDir().getChildFile ("soundfonts.txt"); }

    inline juce::StringArray paths()
    {
        juce::StringArray s;
        s.addLines (listFile().loadFileAsString());
        s.trim(); s.removeEmptyStrings(); s.removeDuplicates (true);
        return s;
    }
    inline void save (const juce::StringArray& s)
    {
        userDir().createDirectory();
        listFile().replaceWithText (s.joinIntoString ("\r\n"));
    }

    struct AddResult { juce::StringArray added, already, rejected; };   // rejected: "name: reason"

    // files and folders (recursively, *.sf2)
    inline AddResult add (const juce::Array<juce::File>& items)
    {
        AddResult r;
        auto list = paths();
        auto consider = [&] (const juce::File& f)
        {
            const auto info = sf2::get (f.getFullPathName());
            if (! info->ok()) { r.rejected.add (f.getFileName() + ": " + info->error); return; }
            if (list.contains (f.getFullPathName(), true)) { r.already.add (f.getFileName()); return; }
            list.add (f.getFullPathName()); r.added.add (f.getFullPathName());
        };
        for (auto& it : items)
        {
            if (it.isDirectory())
            {
                auto files = it.findChildFiles (juce::File::findFiles, true, "*.sf2");
                files.sort();
                for (auto& f : files) consider (f);
            }
            else if (it.existsAsFile()) consider (it);
        }
        save (list);
        return r;
    }

    inline void remove (const juce::String& path)
    {
        auto list = paths();
        list.removeString (path, true);
        save (list);
    }
}
