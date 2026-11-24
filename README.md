# Radically Open Journal

A Pebble app for keeping an RO-DBT diary card. The watch asks the questions at
the end of each day; the phone keeps the history and turns it into spreadsheets.

## What it does

- **Daily survey on the watch.** One question per screen. Up and Down change
  the answer, Select moves on, Back goes to the previous question.
  - Ratings run from 0 to 5 in steps of 0.5. They start at the value from the
    latest day you entered, or at 2.5 if there is none.
  - Yes/No questions toggle with Up or Down and start at the answer from the
    latest day you entered, or No.
- **Questions.** Three are always asked: urge to commit suicide (0-5), taken
  prescribed meds (yes/no), other drugs & alcohol (yes/no). After that come two
  sections you fill in yourself from the settings page on the phone:
  *Thoughts, Sensations, Emotions* and *Social Signaling* (up to 24 questions
  between them, each either a 0-5 rating or yes/no).
- **Reminder.** At the time you pick the watch buzzes and offers Snooze (Up),
  Start (Select) or Ignore (Down). If nobody answers within two minutes it
  snoozes by itself, up to three times. It uses the watch's wakeup alarm, so it
  works with the phone out of range.
- **Reports.** The settings page builds a spreadsheet for any week, or for the
  entire dataset: one row per day (`Mon Nov 23rd 2026`), one column per
  question. Download it as `.csv`, share it, or copy it and paste straight into
  a spreadsheet.

A card filled in before 4am counts for the previous day, so snoozing past
midnight does not put the entry on the wrong date.

**Missed days.** On the home screen, Up steps back through the last 30 days and
Down comes forward again; Select fills in the day shown. The home screen says
whether each day is filled in. The watch only keeps the answers of the latest
day, so opening today's card again edits it, while opening an older filled-in
day starts it over (pre-filled from the latest day) and replaces it on save.

## Building

The dev shell provides the Pebble SDK, compiler and emulator:

```sh
nix develop
pebble build
pebble install --emulator basalt     # or aplite, chalk, diorite, emery, flint, gabbro
pebble install --phone <phone ip>    # with the developer connection enabled in the phone app
```

The built app is `build/radically-open-journal.pbw`, which can also be
sideloaded from the phone.

To work on the settings page, open `config/index.html` in a browser. Outside the
phone app it loads sample data and Save only shows what would be stored.

## How it fits together

| Path | Role |
| --- | --- |
| `src/c/main.c` | App start-up and the home screen |
| `src/c/survey.c` | The question screens and the "Saved" screen |
| `src/c/reminder.c` | Wakeup scheduling and the reminder screen |
| `src/c/comm.c` | Messages to and from the phone |
| `src/c/data.c` | Watch storage and date helpers |
| `src/c/ui.c` | Shared drawing helpers |
| `src/pkjs/index.js` | Phone side: stores history and settings, syncs the watch |
| `config/index.html` | Settings and reports page shown in the phone app |

The phone holds the full history (in the Pebble app's storage for this
watchapp). The watch keeps only the question list, the latest day's entry, which recent
days are filled in, and up to
ten entries it has not yet managed to hand over, which it sends the next time
the app is opened with the phone connected. Nothing is sent to any server: the
settings page is bundled into the app (`wscript` embeds `config/index.html` at
build time) and reports are generated on the phone.

Removing a question keeps its past answers; it still gets a column in any
report covering days that have answers for it. A question's answer type cannot
be changed after it is saved, for the same reason. Remove it and add a new one.
