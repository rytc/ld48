
struct Dialog_Sequence_Definition {
    u32 dialog_count;
    const char *lines[10];
};

enum Dialog_Sequence_ID {
    DIALOG_SEQUENCE_0,
    DIALOG_SEQUENCE_2,
    DIALOG_DEKARD,
};

static const Dialog_Sequence_Definition d_sequences[] = {
    {4, {
            "",
            "SB-48: Scanning identity...",
            "SB-48: NAME: Rohn ; SECURITY CLEARANCE: Sufficient",
            "SB-48: Access granted. Welcome to Depar",
        }},
    {2, {
            "",
            "Kaelia: Got any Spice?"
        }},
    {6, {
            "Dekard: ...",
            "Dekard: You're the guy they sent?",
            "Dekard: Well, the ... situation ... is bigger than a one man job.",
            "Dekard: The tunnel system below have been heavily infiltrated.",
            "Dekard: The corpos have been trying to take Depar without making a rukus.",
            "Dekard: Good luck down there, hope you have good insurance."

        }},

};

struct Entity;
struct Dialog_Sequence {
    s32 id;
    s32 line;
    Entity *giver;
};


