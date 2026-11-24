#include "comm.h"
#include "data.h"

static bool s_outbox_busy;
static CommHandler s_synced_handler;
static CommHandler s_config_handler;

static int32_t tuple_int(DictionaryIterator *iter, uint32_t key, int32_t fallback) {
  Tuple *tuple = dict_find(iter, key);
  return tuple ? tuple->value->int32 : fallback;
}

void comm_flush(void) {
  if (s_outbox_busy) {
    return;
  }
  Entry entry;
  if (!data_pending_oldest(&entry)) {
    return;
  }
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_ENTRY_DATE, entry.date);
  dict_write_data(iter, MESSAGE_KEY_ENTRY_DATA, (const uint8_t *)entry.answers,
                  entry.count * sizeof(Answer));
  if (app_message_outbox_send() == APP_MSG_OK) {
    s_outbox_busy = true;
  }
}

static void handle_config_begin(DictionaryIterator *iter) {
  Settings settings = *data_settings();
  settings.reminder_enabled = tuple_int(iter, MESSAGE_KEY_CFG_ENABLED, settings.reminder_enabled) != 0;
  settings.hour = tuple_int(iter, MESSAGE_KEY_CFG_HOUR, settings.hour);
  settings.minute = tuple_int(iter, MESSAGE_KEY_CFG_MINUTE, settings.minute);
  settings.snooze_min = tuple_int(iter, MESSAGE_KEY_CFG_SNOOZE, settings.snooze_min);
  data_set_settings(&settings);
}

static void handle_question(DictionaryIterator *iter) {
  Question question;
  memset(&question, 0, sizeof(question));
  question.id = tuple_int(iter, MESSAGE_KEY_Q_ID, 0);
  question.type = tuple_int(iter, MESSAGE_KEY_Q_TYPE, TYPE_SCALE) == TYPE_BOOL ? TYPE_BOOL : TYPE_SCALE;
  question.section = tuple_int(iter, MESSAGE_KEY_Q_SECTION, SECTION_TSE) == SECTION_SOCIAL
                         ? SECTION_SOCIAL : SECTION_TSE;
  Tuple *label = dict_find(iter, MESSAGE_KEY_Q_LABEL);
  if (label && label->type == TUPLE_CSTRING) {
    strncpy(question.label, label->value->cstring, LABEL_LEN - 1);
  }
  data_stage_custom_question(tuple_int(iter, MESSAGE_KEY_Q_INDEX, -1), &question);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  if (dict_find(iter, MESSAGE_KEY_CFG_BEGIN)) {
    handle_config_begin(iter);
  }
  if (dict_find(iter, MESSAGE_KEY_Q_INDEX)) {
    handle_question(iter);
  }
  Tuple *end = dict_find(iter, MESSAGE_KEY_CFG_END);
  if (end) {
    data_commit_custom_questions(end->value->int32);
    if (s_config_handler) {
      s_config_handler();
    }
  }
  Tuple *ack = dict_find(iter, MESSAGE_KEY_ENTRY_ACK);
  if (ack) {
    data_pending_remove(ack->value->int32);
    if (s_synced_handler) {
      s_synced_handler();
    }
    comm_flush();
  }
  if (dict_find(iter, MESSAGE_KEY_READY)) {
    comm_flush();
  }
}

static void outbox_sent(DictionaryIterator *iter, void *context) {
  s_outbox_busy = false;
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  // The entry stays queued; it is retried when the phone next says it is ready.
  s_outbox_busy = false;
}

void comm_set_synced_handler(CommHandler handler) {
  s_synced_handler = handler;
}

void comm_set_config_handler(CommHandler handler) {
  s_config_handler = handler;
}

void comm_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(256, 256);
}
