// Feeds key sequences from stdin to the Go bamboo-core and prints the
// engine state after every key, in the format compare.py expects.
package main

import (
	"bamboo-core"
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"
)

// Mirrors EngineSetOption in bamboo/bamboo-c.go.
func newEngine(im string, modern, free bool, w2u int) bamboo.IEngine {
	e := bamboo.NewEngine(bamboo.ParseInputMethod(bamboo.InputMethodDefinitions, im), bamboo.EstdFlags)
	flags := bamboo.EstdFlags
	if modern {
		flags &= ^bamboo.EstdToneStyle
	}
	if !free {
		flags &= ^bamboo.EfreeToneMarking
	}
	if w2u == 0 {
		flags &= ^bamboo.Ew2uEnabled
	}
	e.SetFlag(flags)
	e.SetW2UMode(w2u)
	e.SetBracketTransformMode(bamboo.BracketTransformDisabled)
	return e
}

var dictionary = map[string]bool{}

// Mirrors shouldFallbackToEnglish in bamboo/fcitxbambooengine.go with the
// default options: restore invalid words on, dd free style on, no macros.
func fallbackToEnglish(e bamboo.IEngine, checkVnRune bool) bool {
	vn := e.GetProcessedString(bamboo.VietnameseMode | bamboo.LowerCase)
	if vn == "" {
		return false
	}
	if !bamboo.HasAnyVietnameseVower(vn) && (strings.HasSuffix(vn, "d") || strings.ContainsRune(vn, 'đ')) {
		return false
	}
	if checkVnRune && !bamboo.HasAnyVietnameseRune(vn) {
		return false
	}
	return !e.IsValid(false)
}

// Mirrors mustFallbackToEnglish with the dictionary spell check on.
func mustFallbackToEnglish(e bamboo.IEngine) bool {
	vn := e.GetProcessedString(bamboo.VietnameseMode | bamboo.LowerCase)
	if vn == "" || strings.ContainsRune(vn, 'đ') {
		return false
	}
	return !dictionary[vn]
}

func raw(e bamboo.IEngine) string {
	return e.GetProcessedString(bamboo.EnglishMode | bamboo.FullText)
}

// The preedit and the text a space would commit, as getPreeditString and
// getCommitText compute them.
func shownAndCommitted(e bamboo.IEngine) (string, string) {
	shown := e.GetProcessedString(bamboo.PunctuationMode | bamboo.FullText)
	if fallbackToEnglish(e, true) {
		shown = raw(e)
	}
	if bamboo.HasAnyVietnameseRune(shown) && mustFallbackToEnglish(e) {
		return shown, raw(e)
	}
	return shown, shown
}

func row(id string, step int, op string, e bamboo.IEngine) string {
	shown, committed := shownAndCommitted(e)
	return strings.Join([]string{
		id, strconv.Itoa(step), op, shown, committed,
		e.GetProcessedString(bamboo.PunctuationMode | bamboo.FullText),
		raw(e),
		e.GetProcessedString(bamboo.VietnameseMode | bamboo.LowerCase),
		strconv.FormatBool(e.IsValid(false)),
		strconv.FormatBool(e.IsValid(true)),
	}, "\t")
}

// Same parsing as NewDictionary in bamboo/bamboo-c.go.
func loadDictionary(path string) {
	data, err := os.ReadFile(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	for _, line := range strings.Split(string(data), "\n") {
		if line != "" {
			dictionary[strings.ToLower(line)] = true
		}
	}
}

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: go-runner DICTIONARY < cases")
		os.Exit(2)
	}
	loadDictionary(os.Args[1])
	in := bufio.NewScanner(os.Stdin)
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	for in.Scan() {
		f := strings.Split(in.Text(), "\t")
		if len(f) != 6 {
			continue
		}
		w2u, _ := strconv.Atoi(f[4])
		e := newEngine(f[1], f[2] == "1", f[3] == "1", w2u)
		step := 0
		for _, k := range f[5] {
			var op string
			switch k {
			case '<':
				e.RemoveLastChar(true)
				op = "bs"
			case '!':
				e.RestoreLastWord(false)
				op = "restore"
			default:
				if fallbackToEnglish(e, false) {
					op = "en:" + string(k)
					e.ProcessKey(k, bamboo.EnglishMode)
				} else {
					op = "vn:" + string(k)
					e.ProcessKey(k, bamboo.VietnameseMode)
				}
			}
			step++
			fmt.Fprintln(out, row(f[0], step, op, e))
		}
	}
}
