#!/bin/bash

PROMPT_LINES=2

input_buffer=""

init_terminal() {
    # Hide cursor to avoid flickering
    tput civis

    # Scroll enough for the prompt in case we're at the
    # bottom of the screen
    for i in $(seq 1 $PROMPT_LINES); do
        echo
    done

    # Move cursor to the top of the room we just made
    tput cuu $((PROMPT_LINES + 1))

    # Save cursor position
    tput sc

    # Exclude prompt from scrollable area
    tput csr 0 "$(($(tput lines) - PROMPT_LINES - 1))"

    # Changing scrollable area moves cursor, so put it back
    tput rc
}

restore_terminal() {
    clear_status
    clear_prompt

    tput csr 0 "$(tput lines)"
    tput rc
    tput cnorm
}

eval_javascript_in_gnome_shell() {
    json=$(mktemp)
    busctl call --user --json=short               \
                org.gnome.Shell                   \
                /org/gnome/Shell                  \
                org.gnome.Shell                   \
                Eval "s" "$1" > "$json"
    result=$(jq '.data[0]' < "$json")
    output=$(jq '.data[1] | select(. != null and . != "") | try fromjson catch .' < "$json")
    rm -f "$json"

    if [ "$result" = "false" ]; then
        echo -e "\e[31m${output:1:-1}\e[0m" > /dev/stderr
        return 1
    fi

    echo "$output"
    return 0
}

eval_javascript_in_looking_glass() {
    # encode the text so we can side-step complicated escaping rules
    ENCODED_TEXT=$(echo -n "$1" | hexdump -v -e '/1 "%02x"')

    eval_javascript_in_gnome_shell "
        const GLib = imports.gi.GLib;
        Main.createLookingGlass();
        const results = Main.lookingGlass._resultsArea;
        Main.lookingGlass._entry.text = '${ENCODED_TEXT}'.replace(/([0-9a-fA-F]{2})/g, (_, h) => String.fromCharCode(parseInt(h, 16)));
        Main.lookingGlass._entry.clutter_text.activate();
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 125, () => {
            const index = results.get_n_children() - 1;
            if (index < 0)
                return;
            const resultsActor = results.get_children()[index];
            const output = \`\${resultsActor.get_children()[1].get_children()[0].text}\${resultsActor.get_children()[1].get_children()[1].get_children()[0].text}\`;
            Main.lookingGlass._lastEncodedResult = output.split('').map(char => char.charCodeAt(0).toString(16).padStart(2, '0')).join('');
        });
    " > /dev/null

    if [ $? -ne 0 ]; then
        return
    fi

    sleep .250

    OUTPUT=$(echo -e $(eval_javascript_in_gnome_shell 'Main.lookingGlass._lastEncodedResult;' | sed 's/\([[:xdigit:]][[:xdigit:]]\)/\\x\1/g'))

    if [ -z "$OUTPUT" ]; then
        echo -e "\e[31mCould not fetch result from call\e[0m" > /dev/stderr
        return
    fi

    eval_javascript_in_gnome_shell "delete Main.lookingGlass._lastEncodedResult;" > /dev/null

    echo ">>> $1"
    echo "${OUTPUT}"
}

jump_to_prompt() {
    # Move to the bottom of the terminal
    tput cup $(($(tput lines) - PROMPT_LINES)) 0
}

clear_prompt() {
    jump_to_prompt
    tput el
}

jump_to_status() {
    # Move to just below the prompt
    tput cup $(($(tput lines) - PROMPT_LINES + 1)) 0
}

clear_status() {
    jump_to_status
    tput el
}

print_status_line() {
    jump_to_status
    echo -ne "Type quit to exit, ^G to inspect, ^L to clear screen"
    tput rc
}

clear_screen() {
    clear
    ask_user_for_input
}

ask_user_for_input() {
    # Save cursor position
    tput sc

    clear_prompt

    tput cnorm
    read -i "$READLINE_LINE" -p ">>> " -re input_buffer
    STATUS="$?"
    tput civis

    [ $STATUS != 0 ] && exit

    if [ "$input_buffer" = "quit" -o "$input_buffer" = "q" -o "$input_buffer" = "exit" ]; then
        exit
    fi

    # Save input to history
    history -s "$input_buffer"

    # Move cursor back to saved position before output
    tput rc
}

quit_message() {
    print_status_line
    ask_user_for_input
}

load_history() {

    while IFS= read -r line; do
        history -s "$line"
    done < <(eval_javascript_in_gnome_shell 'Main.lookingGlass._history._history.join("\n");' | jq -r '. | select(. != null and . != "") | tostring')
}

check_for_unsafe_mode() {
    unsafe_mode=$(eval_javascript_in_gnome_shell 'global.context.unsafe_mode')

    if [ "$unsafe_mode" != "true" ]; then
        echo -e "Please enable unsafe-mode in the Flags tab of looking glass." > /dev/stderr
        exit
    fi
}

eval_autocomplete_javascript() {
    ENCODED_TEXT=$(echo -n "$1" | hexdump -v -e '/1 "%02x"')
    OUTPUT=$(eval_javascript_in_gnome_shell '
        const AsyncFunction = async function () {}.constructor;

        const command = `
            const JsParse = await import("resource:///org/gnome/shell/misc/jsParse.js");

            function getGlobalState() {
                const keywords = ["true", "false", "null", "new"];
                const windowProperties = Object.getOwnPropertyNames(globalThis).filter(
                    a => a.charAt(0) !== "_");
                const headerProperties = JsParse.getDeclaredConstants(commandHeader);

                return keywords.concat(windowProperties).concat(headerProperties);
            }

            const commandHeader = '"'"'const {Clutter, Gio, GLib, GObject, Meta, Shell, St} = imports.gi; const Main = await import("resource:///org/gnome/shell/ui/main.js"); const inspect = Main.lookingGlass.inspect.bind(Main.lookingGlass); const it = Main.lookingGlass.getIt(); const r = Main.lookingGlass.getResult.bind(Main.lookingGlass);'"'"';

            const text="'${ENCODED_TEXT}'".replace(/([0-9a-fA-F]{2})/g, (_, h) => String.fromCharCode(parseInt(h, 16)));
            const completions = await JsParse.getCompletions(text, commandHeader, getGlobalState());
            return {
                "completions": completions[0],
                "attrHead": completions[1]
            };`;
        AsyncFunction(command)();
    ')
    if [ $? = 1 ]; then
        echo fail
    fi
    echo "$OUTPUT"
}

autocomplete() {
    RESULT="$(eval_autocomplete_javascript "$READLINE_LINE")"
    COMPLETIONS="$(echo "$RESULT" | jq '.completions[]' | sed 's/\"//g')"
    ATTR_HEAD=$(echo "$RESULT" | jq '.attrHead' | sed 's/\"//g')
    N_COMPLETIONS=$(echo "$COMPLETIONS" | wc -w)
    if [ $N_COMPLETIONS = 0 ]; then
        return
    elif [ $N_COMPLETIONS = 1 ]; then
        TO_ADD=$(echo $COMPLETIONS | sed s/$ATTR_HEAD//)
        READLINE_LINE+="$TO_ADD"
        (( READLINE_POINT += $(echo "$TO_ADD" | wc -c) ))
    else
        tput rc
        echo
        echo "$COMPLETIONS" | pr -T -2 -o 4
        ask_user_for_input
    fi
}

inspect() {
    eval_javascript_in_gnome_shell '
        const AsyncFunction = async function () {}.constructor;

        delete Main.lookingGlass._lastInspection;
        const command = `
            const Main = await import("resource:///org/gnome/shell/ui/main.js");
            const LookingGlass = await import("resource:///org/gnome/shell/ui/lookingGlass.js");

            Main.lookingGlass._inspector = new LookingGlass.Inspector(Main.lookingGlass);
            const inspector = Main.lookingGlass._inspector;

            inspector.connectObject("target", (i, obj, stageX, stageY) => {
                let command = "inspect(" + Math.round(stageX) + ", " + Math.round(stageY) + ")";
                Main.lookingGlass._lastInspection = command;
            },
            "closed", () => {
                delete Main.lookingGlass._inspector;
            });
        `;
        AsyncFunction(command)();
    '

    while sleep .250; do
        finished=$(eval_javascript_in_gnome_shell 'Main.lookingGlass._inspector? false : true')
        last_inspection=$(eval_javascript_in_gnome_shell 'Main.lookingGlass._lastInspection' | jq -r)

        [ "$finished" = "true" ] && break
        [ -n "$last_inspection" ] && break
    done

    tput rc
    [ -n "$last_inspection" ] && eval_javascript_in_looking_glass "$last_inspection"
    eval_javascript_in_gnome_shell 'delete Main.lookingGlass._lastInspection; Main.lookingGlass.setBorderPaintTarget(null);' > /dev/null

    ask_user_for_input
}

main_loop() {
    bind -x '"\a"':inspect 2> /dev/null
    bind -x '"\t"':autocomplete 2> /dev/null
    bind -x '"\f"':clear_screen 2> /dev/null
    while true; do
        ask_user_for_input
        eval_javascript_in_looking_glass "$input_buffer"
    done
}

check_for_unsafe_mode

trap 'quit_message' SIGINT
trap restore_terminal EXIT

init_terminal
load_history
print_status_line
main_loop
restore_terminal
