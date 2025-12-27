#include "client_bridge.h"
#include <algorithm>
#include <cstring>
#include <gtk/gtk.h>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Forward declaration for chat conversation handler (after including gtk)
static void on_open_conversation_clicked(GtkWidget *widget, gpointer data);

// =========================================================
// 1. BIẾN TOÀN CỤC & CẤU TRÚC DỮ LIỆU
// =========================================================

// Widget cơ bản
GtkWidget *window;
GtkWidget *vbox_login;
GtkWidget *entry_email;
GtkWidget *entry_password;
GtkWidget *lbl_status;

// Widget chức năng
GtkWidget *entry_chat_msg;
GtkWidget *entry_chat_user;

// Dữ liệu phiên bản
extern std::string sessionToken;
extern std::string currentLevel;

// Cấu trúc câu hỏi thi
struct Question {
  std::string id;
  std::string type; // multiple_choice, fill_blank, sentence_order
  std::string content;
  std::string a, b, c, d;         // for MCQ
  std::vector<std::string> words; // for sentence_order
};

struct GameItem {
  std::string id;
  std::string title;
  std::string type;
  std::string level;
};

// Quản lý bài thi
std::string currentTestId = "";
std::vector<Question> currentQuestions;
std::map<int, std::string> userAnswers;
int currentQuestionIdx = 0;

// Widget bài thi
GtkWidget *lbl_q_number;
GtkWidget *lbl_q_content;
GtkWidget *rb_opt_a, *rb_opt_b, *rb_opt_c, *rb_opt_d;
GtkWidget *btn_test_action;
GtkWidget *entry_answer_text; // for fill_blank & sentence_order
GtkWidget *lbl_words;         // shows words for sentence_order

// Conversation auto-refresh state
struct ConversationState {
  GtkWidget *dialog;
  GtkWidget *box_msgs;
  std::string recipientId;
  std::string recipientLabel;
  int messageCount;
  guint timeout_id;
};
static ConversationState *g_conv_state = nullptr;

// Khai báo tên hàm
void show_main_menu();
void show_test_dialog();
void show_game_dialog();

// Escape JSON for small payloads
static std::string escape_json(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

// =========================================================
// MEDIA PLAYBACK HELPERS
// =========================================================

// Play video using xdg-open (opens default browser/player)
static void play_video(const std::string &url) {
  if (url.empty()) {
    GtkWidget *msg = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "No video available for this lesson.");
    gtk_dialog_run(GTK_DIALOG(msg));
    gtk_widget_destroy(msg);
    return;
  }

  g_print("[VIDEO] Playing: %s\n", url.c_str());

  // Use xdg-open for URLs (opens default browser for YouTube, etc.)
  // Use mpv or vlc for local files
  std::string command;
  if (url.find("http://") == 0 || url.find("https://") == 0) {
    command = "xdg-open \"" + url + "\" &";
  } else {
    // Local file - try mpv first, then vlc, then xdg-open
    command = "(which mpv && mpv --no-terminal \"" + url +
              "\") || (which vlc && vlc \"" + url +
              "\") || xdg-open \"" + url + "\" &";
  }

  int result = system(command.c_str());
  if (result != 0) {
    GtkWidget *msg = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
        "Failed to play video.\nURL: %s", url.c_str());
    gtk_dialog_run(GTK_DIALOG(msg));
    gtk_widget_destroy(msg);
  }
}

// Play audio using available audio players
static void play_audio(const std::string &url) {
  if (url.empty()) {
    GtkWidget *msg = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "No audio available for this lesson.");
    gtk_dialog_run(GTK_DIALOG(msg));
    gtk_widget_destroy(msg);
    return;
  }

  g_print("[AUDIO] Playing: %s\n", url.c_str());

  std::string command;
  if (url.find("http://") == 0 || url.find("https://") == 0) {
    // For URLs, use mpv or vlc (audio-only mode)
    command = "(which mpv && mpv --no-video --no-terminal \"" + url +
              "\") || (which vlc && cvlc --play-and-exit \"" + url +
              "\") || xdg-open \"" + url + "\" &";
  } else {
    // Local file - try paplay (PulseAudio), aplay, mpv, or vlc
    command = "(which paplay && paplay \"" + url +
              "\") || (which aplay && aplay \"" + url +
              "\") || (which mpv && mpv --no-video --no-terminal \"" + url +
              "\") || (which vlc && cvlc --play-and-exit \"" + url + "\") &";
  }

  int result = system(command.c_str());
  if (result != 0) {
    GtkWidget *msg = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
        "Failed to play audio.\nFile: %s", url.c_str());
    gtk_dialog_run(GTK_DIALOG(msg));
    gtk_widget_destroy(msg);
  }
}

// Callback for video button
static void on_play_video_clicked(GtkWidget *widget, gpointer data) {
  const char *url = (const char *)data;
  if (url) {
    play_video(std::string(url));
  }
}

// Callback for audio button
static void on_play_audio_clicked(GtkWidget *widget, gpointer data) {
  const char *url = (const char *)data;
  if (url) {
    play_audio(std::string(url));
  }
}

// =========================================================
// 2. CÁC HÀM TIỆN ÍCH (HELPER)
// =========================================================

void update_question_ui() {
  if (currentQuestions.empty())
    return;

  Question &q = currentQuestions[currentQuestionIdx];

  // Cập nhật tiêu đề và nội dung
  std::string title = "Câu hỏi " + std::to_string(currentQuestionIdx + 1) +
                      "/" + std::to_string(currentQuestions.size());
  gtk_label_set_text(GTK_LABEL(lbl_q_number), title.c_str());
  gtk_label_set_text(GTK_LABEL(lbl_q_content), q.content.c_str());

  // Hiển thị theo loại câu hỏi
  bool isMCQ = (q.type == "multiple_choice");
  gtk_widget_set_visible(rb_opt_a, isMCQ);
  gtk_widget_set_visible(rb_opt_b, isMCQ);
  gtk_widget_set_visible(rb_opt_c, isMCQ);
  gtk_widget_set_visible(rb_opt_d, isMCQ);
  gtk_widget_set_visible(entry_answer_text, !isMCQ);

  if (q.type == "sentence_order") {
    std::string joined;
    for (size_t i = 0; i < q.words.size(); ++i) {
      if (i)
        joined += "  |  ";
      joined += q.words[i];
    }
    std::string hint = "Sắp xếp các từ thành câu đúng:\n\n" + joined +
                       "\n\nGõ câu hoàn chỉnh bên dưới:";
    gtk_label_set_text(GTK_LABEL(lbl_words), hint.c_str());
    gtk_widget_set_visible(lbl_words, true);
  } else {
    gtk_widget_set_visible(lbl_words, false);
  }

  if (isMCQ) {
    gtk_button_set_label(GTK_BUTTON(rb_opt_a), q.a.c_str());
    gtk_button_set_label(GTK_BUTTON(rb_opt_b), q.b.c_str());
    gtk_button_set_label(GTK_BUTTON(rb_opt_c), q.c.c_str());
    gtk_button_set_label(GTK_BUTTON(rb_opt_d), q.d.c_str());
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_opt_a), TRUE);
  } else {
    gtk_entry_set_text(GTK_ENTRY(entry_answer_text), "");
  }

  // Đổi tên nút ở câu cuối
  if (currentQuestionIdx == currentQuestions.size() - 1) {
    gtk_button_set_label(GTK_BUTTON(btn_test_action), "Nộp bài (Submit) 🏁");
  } else {
    gtk_button_set_label(GTK_BUTTON(btn_test_action), "Tiếp theo ➡️");
  }
}

// =========================================================
// 3. CHỨC NĂNG: LÀM BÀI TEST (Parser & Logic)
// =========================================================

// [QUAN TRỌNG] Hàm này bóc tách JSON thật từ Server
void parse_test_data(std::string jsonResponse) {
  currentQuestions.clear();
  userAnswers.clear();
  currentQuestionIdx = 0;

  g_print("DEBUG: Parsing test response...\n");

  // 1. Tìm TestID
  std::regex re_testId("\"testId\"\\s*:\\s*\"([^\"]+)\"");
  std::smatch m_id;
  if (std::regex_search(jsonResponse, m_id, re_testId) && m_id.size() > 1) {
    currentTestId = m_id.str(1);
    g_print("DEBUG: Test ID = %s\n", currentTestId.c_str());
  } else {
    currentTestId = "unknown_test";
  }

  // 2. Parse questions array - handle actual server format
  // Look for "questions":[...] and parse each question object
  size_t questionsPos = jsonResponse.find("\"questions\"");
  if (questionsPos == std::string::npos) {
    g_print("ERROR: No 'questions' field found in response\n");
    currentQuestions.push_back({"err",
                                "Server response missing 'questions' field",
                                "A", "B", "C", "D"});
    return;
  }

  // Simple parser for question objects (preserve option order a,b,c,d)
  size_t pos = questionsPos;
  int questionCount = 0;
  while ((pos = jsonResponse.find("\"questionId\"", pos)) !=
         std::string::npos) {
    // Extract questionId
    size_t idStart = jsonResponse.find("\"", pos + 13);
    size_t idEnd = jsonResponse.find("\"", idStart + 1);
    std::string qId = jsonResponse.substr(idStart + 1, idEnd - idStart - 1);

    // Extract question content
    size_t contentPos = jsonResponse.find("\"question\"", pos);
    if (contentPos == std::string::npos)
      contentPos = jsonResponse.find("\"content\"", pos);
    if (contentPos != std::string::npos && contentPos < pos + 1000) {
      size_t contentStart = jsonResponse.find("\"", contentPos + 10);
      size_t contentEnd = jsonResponse.find("\"", contentStart + 1);
      std::string content =
          jsonResponse.substr(contentStart + 1, contentEnd - contentStart - 1);

      // Unescape newlines
      size_t npos = 0;
      while ((npos = content.find("\\n", npos)) != std::string::npos) {
        content.replace(npos, 2, "\n");
        npos += 1;
      }

      // Parse type
      std::string qType = "multiple_choice";
      size_t typePos = jsonResponse.find("\"type\"", pos);
      if (typePos != std::string::npos && typePos < pos + 500) {
        size_t tStart = jsonResponse.find('"', typePos + 6);
        size_t tEnd = jsonResponse.find('"', tStart + 1);
        if (tStart != std::string::npos && tEnd != std::string::npos) {
          qType = jsonResponse.substr(tStart + 1, tEnd - tStart - 1);
        }
      }

      // Parse options array:
      // "options":[{"id":"a","text":"..."},{"id":"b","text":"..."}]
      std::string optA = "A", optB = "B", optC = "C", optD = "D";
      size_t optionsPos = jsonResponse.find("\"options\"", contentPos);
      if (optionsPos != std::string::npos && optionsPos < pos + 1500) {
        // Iterate through options array sequentially to preserve order
        size_t arrStart = jsonResponse.find('[', optionsPos);
        size_t arrEnd = jsonResponse.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
          int idx = 0;
          size_t oPos = arrStart;
          while (oPos < arrEnd && idx < 4) {
            size_t idKey = jsonResponse.find("\"id\"", oPos);
            if (idKey == std::string::npos || idKey > arrEnd)
              break;
            size_t idStart2 = jsonResponse.find('"', idKey + 4);
            size_t idEnd2 = jsonResponse.find('"', idStart2 + 1);
            char idChar = 'a';
            if (idStart2 != std::string::npos && idEnd2 != std::string::npos) {
              if (idEnd2 > idStart2 + 1)
                idChar = jsonResponse[idStart2 + 1];
            }
            size_t textKey = jsonResponse.find("\"text\"", idKey);
            if (textKey == std::string::npos || textKey > arrEnd)
              break;
            size_t tStart = jsonResponse.find('"', textKey + 6);
            size_t tEnd = jsonResponse.find('"', tStart + 1);
            std::string optText =
                (tStart != std::string::npos && tEnd != std::string::npos)
                    ? jsonResponse.substr(tStart + 1, tEnd - tStart - 1)
                    : "";
            if (idx == 0 || idChar == 'a')
              optA = optText;
            else if (idx == 1 || idChar == 'b')
              optB = optText;
            else if (idx == 2 || idChar == 'c')
              optC = optText;
            else if (idx == 3 || idChar == 'd')
              optD = optText;
            idx++;
            oPos = tEnd + 1;
          }
        }
      }

      // Parse words for sentence_order
      std::vector<std::string> words;
      size_t wordsPos = jsonResponse.find("\"words\"", contentPos);
      if (wordsPos != std::string::npos && wordsPos < pos + 1500) {
        size_t wStart = jsonResponse.find('[', wordsPos);
        size_t wEnd = jsonResponse.find(']', wStart);
        size_t wp = wStart;
        while (wp != std::string::npos && wp < wEnd) {
          size_t s1 = jsonResponse.find('"', wp + 1);
          size_t s2 = jsonResponse.find('"', s1 + 1);
          if (s1 == std::string::npos || s2 == std::string::npos || s2 > wEnd)
            break;
          words.push_back(jsonResponse.substr(s1 + 1, s2 - s1 - 1));
          wp = s2 + 1;
        }
      }

      currentQuestions.push_back(
          Question{qId, qType, content, optA, optB, optC, optD, words});
      questionCount++;
      g_print("DEBUG: Parsed question %d: %s (A=%s, B=%s, C=%s, D=%s)\n",
              questionCount, qId.c_str(), optA.c_str(), optB.c_str(),
              optC.c_str(), optD.c_str());
    }
    pos = contentPos + 1;
    if (questionCount >= 20)
      break; // Safety limit
  }

  if (currentQuestions.empty()) {
    g_print("ERROR: No questions parsed from response\n");
    currentQuestions.push_back(
        {"err",
         "Could not parse questions from server response\n\nPlease check "
         "server is running and test data exists.",
         "A", "B", "C", "D"});
  } else {
    g_print("SUCCESS: Parsed %d questions\n", (int)currentQuestions.size());
  }
}

static void on_test_action_clicked(GtkWidget *widget, gpointer data) {
  // 1. Lưu đáp án theo loại câu hỏi
  std::string value;
  if (currentQuestions[currentQuestionIdx].type == "multiple_choice") {
    value = "a";
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_opt_b)))
      value = "b";
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_opt_c)))
      value = "c";
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_opt_d)))
      value = "d";
  } else {
    const char *txt = gtk_entry_get_text(GTK_ENTRY(entry_answer_text));
    value = txt ? txt : "";
  }
  userAnswers[currentQuestionIdx] = value;

  // 2. Điều hướng
  if (currentQuestionIdx < currentQuestions.size() - 1) {
    currentQuestionIdx++;
    update_question_ui();
  } else {
    // 3. Nộp bài (grading on server by per-question points)
    std::string answersJson = "[";
    for (int i = 0; i < (int)currentQuestions.size(); i++) {
      const std::string &qId = currentQuestions[i].id;
      const std::string &ans = userAnswers[i]; // "a"/"b"/"c"/"d" or text
      answersJson +=
          "{\"questionId\":\"" + qId + "\",\"answer\":\"" + ans + "\"}";
      if (i < (int)currentQuestions.size() - 1)
        answersJson += ",";
    }
    answersJson += "]";

    std::string jsonRequest =
        "{\"messageType\":\"SUBMIT_TEST_REQUEST\", \"sessionToken\":\"" +
        sessionToken + "\", \"payload\":{\"testId\":\"" + currentTestId +
        "\", \"answers\":" + answersJson + "}}";

    gtk_label_set_text(GTK_LABEL(lbl_q_content), "Đang chấm điểm...");
    while (gtk_events_pending())
      gtk_main_iteration();

    if (sendMessage(jsonRequest)) {
      std::string response = waitForResponse(3000);

      std::string score = "0", grade = "N/A";
      std::regex re_score("\"score\"\\s*:\\s*(\\d+)");
      std::smatch match;
      if (std::regex_search(response, match, re_score) && match.size() > 1)
        score = match.str(1);

      std::regex re_grade("\"grade\"\\s*:\\s*\"([^\"]+)\"");
      if (std::regex_search(response, match, re_grade) && match.size() > 1)
        grade = match.str(1);

      std::string msgContent =
          "KẾT QUẢ TỪ SERVER\n\nĐiểm số: " + score + "\nXếp loại: " + grade +
          "\n\n(Lưu ý: điểm được chấm trên server dựa trên từng câu hỏi, mỗi "
          "câu có điểm 'points' riêng)";
      GtkWidget *msg =
          gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                                 GTK_BUTTONS_OK, "%s", msgContent.c_str());
      gtk_dialog_run(GTK_DIALOG(msg));
      gtk_widget_destroy(msg);

      GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
      if (gtk_widget_is_toplevel(toplevel))
        gtk_widget_destroy(toplevel);
    }
  }
}

void show_test_dialog() {
  // 1. Gửi request lấy đề thi
  std::string jsonRequest =
      "{\"messageType\":\"GET_TEST_REQUEST\", \"sessionToken\":\"" +
      sessionToken +
      "\", \"payload\":{\"testType\":\"multiple_choice\", \"level\":\"" +
      currentLevel + "\"}}";

  sendMessage(jsonRequest);
  std::string response = waitForResponse(3000);

  // 2. Parse dữ liệu thật
  parse_test_data(response);

  // 3. Dựng giao diện
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Làm bài thi", GTK_WINDOW(window), GTK_DIALOG_MODAL, "Hủy",
      GTK_RESPONSE_CLOSE, NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
  gtk_container_add(GTK_CONTAINER(content), vbox);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);

  lbl_q_number = gtk_label_new("");
  gtk_label_set_markup(GTK_LABEL(lbl_q_number),
                       "<span size='large' weight='bold'>Câu hỏi 1</span>");
  gtk_box_pack_start(GTK_BOX(vbox), lbl_q_number, FALSE, FALSE, 0);

  GtkWidget *frame = gtk_frame_new(NULL);
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
  GtkWidget *vbox_q = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(frame), vbox_q);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_q), 15);

  lbl_q_content = gtk_label_new("Loading...");
  gtk_label_set_line_wrap(GTK_LABEL(lbl_q_content), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox_q), lbl_q_content, FALSE, FALSE, 10);

  // For sentence_order hint
  lbl_words = gtk_label_new("");
  gtk_label_set_line_wrap(GTK_LABEL(lbl_words), TRUE);
  gtk_widget_set_visible(lbl_words, FALSE);
  gtk_box_pack_start(GTK_BOX(vbox_q), lbl_words, FALSE, FALSE, 5);

  rb_opt_a = gtk_radio_button_new_with_label(NULL, "A");
  gtk_box_pack_start(GTK_BOX(vbox_q), rb_opt_a, FALSE, FALSE, 0);
  rb_opt_b = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(rb_opt_a), "B");
  gtk_box_pack_start(GTK_BOX(vbox_q), rb_opt_b, FALSE, FALSE, 0);
  rb_opt_c = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(rb_opt_a), "C");
  gtk_box_pack_start(GTK_BOX(vbox_q), rb_opt_c, FALSE, FALSE, 0);
  rb_opt_d = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(rb_opt_a), "D");
  gtk_box_pack_start(GTK_BOX(vbox_q), rb_opt_d, FALSE, FALSE, 0);

  // For fill_blank and sentence_order answer input
  entry_answer_text = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry_answer_text),
                                 "Nhập câu trả lời...");
  gtk_widget_set_visible(entry_answer_text, FALSE);
  gtk_box_pack_start(GTK_BOX(vbox_q), entry_answer_text, FALSE, FALSE, 0);

  btn_test_action = gtk_button_new_with_label("Tiếp theo ➡️");
  g_signal_connect(btn_test_action, "clicked",
                   G_CALLBACK(on_test_action_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_test_action, FALSE, FALSE, 0);

  update_question_ui();

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

// =========================================================
// 4. CHỨC NĂNG: HỌC BÀI (View Lessons - Parse JSON)
// =========================================================

// Static storage for media URLs (freed when dialog closes)
static char *g_current_video_url = nullptr;
static char *g_current_audio_url = nullptr;

void show_lesson_content(const char *lessonId) {
  std::string jsonRequest =
      "{\"messageType\":\"GET_LESSON_DETAIL_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"lessonId\":\"" +
      std::string(lessonId) + "\"}}";

  GtkWidget *loading =
      gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                             GTK_BUTTONS_NONE, "Đang tải bài học...");
  gtk_widget_show_now(loading);
  while (gtk_events_pending())
    gtk_main_iteration();

  if (sendMessage(jsonRequest)) {
    std::string response = waitForResponse(3000);
    gtk_widget_destroy(loading);

    // Parse lesson content
    std::string content = "Không có nội dung.";
    std::regex re_content("\"content\"\\s*:\\s*\"(.*?)\"");
    std::smatch match;
    if (std::regex_search(response, match, re_content) && match.size() > 1) {
      content = match.str(1);
      size_t pos = 0;
      while ((pos = content.find("\\n", pos)) != std::string::npos) {
        content.replace(pos, 2, "\n");
        pos += 1;
      }
    }

    // Parse lesson title
    std::string title = "Bài học";
    std::regex re_title("\"title\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(response, match, re_title) && match.size() > 1) {
      title = match.str(1);
    }

    // Parse video URL
    std::string videoUrl = "";
    std::regex re_video("\"videoUrl\"\\s*:\\s*\"([^\"]*)\"");
    if (std::regex_search(response, match, re_video) && match.size() > 1) {
      videoUrl = match.str(1);
    }

    // Parse audio URL
    std::string audioUrl = "";
    std::regex re_audio("\"audioUrl\"\\s*:\\s*\"([^\"]*)\"");
    if (std::regex_search(response, match, re_audio) && match.size() > 1) {
      audioUrl = match.str(1);
    }

    g_print("[LESSON] Title: %s, Video: %s, Audio: %s\n", title.c_str(),
            videoUrl.empty() ? "(none)" : videoUrl.c_str(),
            audioUrl.empty() ? "(none)" : audioUrl.c_str());

    // Free previous URLs and store new ones
    if (g_current_video_url) {
      free(g_current_video_url);
      g_current_video_url = nullptr;
    }
    if (g_current_audio_url) {
      free(g_current_audio_url);
      g_current_audio_url = nullptr;
    }
    if (!videoUrl.empty()) {
      g_current_video_url = strdup(videoUrl.c_str());
    }
    if (!audioUrl.empty()) {
      g_current_audio_url = strdup(audioUrl.c_str());
    }

    // Create dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        title.c_str(), GTK_WINDOW(window), GTK_DIALOG_MODAL, "Đóng",
        GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 650, 550);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 10);
    gtk_box_pack_start(GTK_BOX(content_area), main_vbox, TRUE, TRUE, 0);

    // Media buttons frame
    bool hasVideo = !videoUrl.empty();
    bool hasAudio = !audioUrl.empty();

    if (hasVideo || hasAudio) {
      GtkWidget *media_frame = gtk_frame_new("Media");
      gtk_box_pack_start(GTK_BOX(main_vbox), media_frame, FALSE, FALSE, 0);

      GtkWidget *media_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_container_set_border_width(GTK_CONTAINER(media_box), 10);
      gtk_container_add(GTK_CONTAINER(media_frame), media_box);

      // Video button
      if (hasVideo) {
        GtkWidget *btn_video = gtk_button_new_with_label("Play Video");
        GtkWidget *video_icon = gtk_image_new_from_icon_name(
            "video-x-generic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(btn_video), video_icon);
        gtk_button_set_always_show_image(GTK_BUTTON(btn_video), TRUE);
        g_signal_connect(btn_video, "clicked",
                         G_CALLBACK(on_play_video_clicked),
                         (gpointer)g_current_video_url);
        gtk_box_pack_start(GTK_BOX(media_box), btn_video, FALSE, FALSE, 0);
      }

      // Audio button
      if (hasAudio) {
        GtkWidget *btn_audio = gtk_button_new_with_label("Play Audio");
        GtkWidget *audio_icon = gtk_image_new_from_icon_name(
            "audio-x-generic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(btn_audio), audio_icon);
        gtk_button_set_always_show_image(GTK_BUTTON(btn_audio), TRUE);
        g_signal_connect(btn_audio, "clicked",
                         G_CALLBACK(on_play_audio_clicked),
                         (gpointer)g_current_audio_url);
        gtk_box_pack_start(GTK_BOX(media_box), btn_audio, FALSE, FALSE, 0);
      }

      // Media status label
      std::string mediaInfo = "";
      if (hasVideo)
        mediaInfo += "Video available  ";
      if (hasAudio)
        mediaInfo += "Audio available";
      GtkWidget *lbl_media = gtk_label_new(mediaInfo.c_str());
      gtk_box_pack_end(GTK_BOX(media_box), lbl_media, FALSE, FALSE, 0);
    }

    // Text content in scrolled window
    GtkWidget *text_frame = gtk_frame_new("Nội dung bài học");
    gtk_box_pack_start(GTK_BOX(main_vbox), text_frame, TRUE, TRUE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(text_frame), scroll);

    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 15);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 15);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tv), 10);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(buffer, content.c_str(), -1);
    gtk_container_add(GTK_CONTAINER(scroll), tv);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}

static void on_lesson_btn_clicked(GtkWidget *widget, gpointer data) {
  char *lessonId = (char *)data;
  show_lesson_content(lessonId);
}

void show_lessons_dialog() {
  std::string jsonRequest =
      "{\"messageType\":\"GET_LESSONS_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"level\":\"" + currentLevel +
      "\", \"topic\":\"\", \"page\":1}}";

  if (sendMessage(jsonRequest)) {
    std::string response = waitForResponse(4000);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Chọn bài học", GTK_WINDOW(window), GTK_DIALOG_MODAL, "Đóng",
        GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 500);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *lbl =
        gtk_label_new(("Danh sách bài học (" + currentLevel + ")").c_str());
    gtk_box_pack_start(GTK_BOX(content_area), lbl, FALSE, FALSE, 10);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(content_area), scrolled, TRUE, TRUE, 0);
    GtkWidget *vbox_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scrolled), vbox_list);

    try {
      std::regex re_lesson("\"lessonId\"\\s*:\\s*\"([^\"]+)\"[^}]*\"title\"\\s*"
                           ":\\s*\"([^\"]+)\"");
      auto words_begin =
          std::sregex_iterator(response.begin(), response.end(), re_lesson);
      auto words_end = std::sregex_iterator();
      int count = 0;
      for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        GtkWidget *btn = gtk_button_new_with_label(match.str(2).c_str());
        g_signal_connect(btn, "clicked", G_CALLBACK(on_lesson_btn_clicked),
                         strdup(match.str(1).c_str()));
        gtk_box_pack_start(GTK_BOX(vbox_list), btn, FALSE, FALSE, 0);
        count++;
      }
      if (count == 0)
        gtk_box_pack_start(GTK_BOX(vbox_list),
                           gtk_label_new("Chưa có bài học nào."), FALSE, FALSE,
                           20);
    } catch (...) {
      gtk_box_pack_start(GTK_BOX(vbox_list), gtk_label_new("Lỗi đọc dữ liệu."),
                         FALSE, FALSE, 20);
    }

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}

// =========================================================
// 5. CHỨC NĂNG CƠ BẢN KHÁC (Chat, Set Level, Login...)
// =========================================================

static void on_send_chat_clicked(GtkWidget *widget, gpointer data) {
  const char *msg = gtk_entry_get_text(GTK_ENTRY(entry_chat_msg));
  if (strlen(msg) == 0)
    return;

  // Get selected user ID from combo box
  const char *recipient =
      gtk_combo_box_get_active_id(GTK_COMBO_BOX(entry_chat_user));
  if (!recipient) {
    GtkWidget *error =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, "Please select a contact");
    gtk_dialog_run(GTK_DIALOG(error));
    gtk_widget_destroy(error);
    return;
  }

  std::string json =
      "{\"messageType\":\"SEND_MESSAGE_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"recipientId\":\"" +
      std::string(recipient) + "\", \"messageContent\":\"" + std::string(msg) +
      "\"}}";
  if (sendMessage(json)) {
    std::string response = waitForResponse(1000);
    gtk_entry_set_text(GTK_ENTRY(entry_chat_msg), "");

    // Show success message
    GtkWidget *success =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                               GTK_BUTTONS_OK, "Message sent!");
    gtk_dialog_run(GTK_DIALOG(success));
    gtk_widget_destroy(success);
    g_print("Sent message to %s\n", recipient);
  }
}

void show_chat_dialog() {
  // First get contact list
  std::string jsonRequest =
      "{\"messageType\":\"GET_CONTACT_LIST_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{}}";

  GtkWidget *loading =
      gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                             GTK_BUTTONS_NONE, "Loading contacts...");
  gtk_widget_show_now(loading);
  while (gtk_events_pending())
    gtk_main_iteration();

  if (!sendMessage(jsonRequest)) {
    gtk_widget_destroy(loading);
    GtkWidget *error =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, "Failed to load contacts");
    gtk_dialog_run(GTK_DIALOG(error));
    gtk_widget_destroy(error);
    return;
  }

  std::string response = waitForResponse(3000);
  gtk_widget_destroy(loading);

  // Parse contacts list
  std::vector<std::pair<std::string, std::string>> contacts; // (userId, name)
  size_t pos = 0;
  g_print("DEBUG: Parsing contacts from response...\n");
  while ((pos = response.find("\"userId\"", pos)) != std::string::npos) {
    size_t idStart = response.find("\"", pos + 9);
    size_t idEnd = response.find("\"", idStart + 1);
    std::string userId = response.substr(idStart + 1, idEnd - idStart - 1);

    // Server sends "fullName" not "fullname"
    size_t namePos = response.find("\"fullName\"", pos);
    if (namePos == std::string::npos) {
      namePos = response.find("\"fullname\"", pos); // fallback
    }
    if (namePos != std::string::npos && namePos < pos + 300) {
      size_t nameStart = response.find("\"", namePos + 11);
      size_t nameEnd = response.find("\"", nameStart + 1);
      std::string name =
          response.substr(nameStart + 1, nameEnd - nameStart - 1);
      contacts.push_back({userId, name});
      g_print("DEBUG: Found contact: %s (%s)\n", name.c_str(), userId.c_str());
    }
    pos = idEnd;
  }
  g_print("DEBUG: Total contacts found: %d\n", (int)contacts.size());

  if (contacts.empty()) {
    GtkWidget *error =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                               GTK_BUTTONS_OK, "No contacts available");
    gtk_dialog_run(GTK_DIALOG(error));
    gtk_widget_destroy(error);
    return;
  }

  // Show chat dialog with contact selector and conversation popup
  GtkWidget *dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(dialog), "Chat Room");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 300);
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(dialog), vbox);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 10);

  gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("Select contact:"), FALSE,
                     FALSE, 0);

  // Create combo box with contacts
  GtkWidget *combo = gtk_combo_box_text_new();
  for (auto &contact : contacts) {
    std::string displayText = contact.second + " (" + contact.first + ")";
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), contact.first.c_str(),
                              displayText.c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
  gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 0);

  // Store combo reference in entry_chat_user data
  entry_chat_user = combo;

  gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("Message:"), FALSE, FALSE, 0);
  entry_chat_msg = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(vbox), entry_chat_msg, FALSE, FALSE, 0);

  GtkWidget *btn = gtk_button_new_with_label("Send 🚀");
  g_signal_connect(btn, "clicked", G_CALLBACK(on_send_chat_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 0);

  // Button to open conversation window
  GtkWidget *btn_open = gtk_button_new_with_label("Open Conversation 💬");
  g_signal_connect(btn_open, "clicked",
                   G_CALLBACK(on_open_conversation_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), btn_open, FALSE, FALSE, 0);
  gtk_widget_show_all(dialog);
}

// Forward declaration for add_chat_bubble
static void add_chat_bubble(GtkWidget *box, const std::string &content,
                            bool isMine, const std::string &senderLabel);

// Struct to pass data to the send callback
struct ConversationContext {
  GtkWidget *box_msgs;
  GtkWidget *entry;
  std::string recipientId;
  std::string recipientLabel;
};

// Auto-refresh function: fetches new messages and updates UI
static gboolean refresh_conversation_messages(gpointer data) {
  if (!g_conv_state || !g_conv_state->dialog)
    return FALSE; // Stop if window closed

  // Fetch chat history again
  std::string req = std::string("{\"messageType\":\"GET_CHAT_HISTORY_REQUEST\", "
                                "\"sessionToken\":\"") +
                    sessionToken + "\", \"payload\":{\"recipientId\":\"" +
                    g_conv_state->recipientId + "\"}}";
  if (!sendMessage(req))
    return TRUE; // Keep trying

  std::string resp = waitForResponse(1000);

  // Count messages
  int newCount = 0;
  size_t pos = 0;
  while ((pos = resp.find("\"senderId\"", pos)) != std::string::npos) {
    newCount++;
    pos += 10;
  }

  // If new messages, show notification and refresh
  if (newCount > g_conv_state->messageCount) {
    g_print("[CHAT] New messages! %d -> %d\n", g_conv_state->messageCount,
            newCount);
    g_conv_state->messageCount = newCount;

    // Show notification
    GtkWidget *notif = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "📬 New message from %s!", g_conv_state->recipientLabel.c_str());
    gtk_dialog_run(GTK_DIALOG(notif));
    gtk_widget_destroy(notif);

    // Clear and rebuild message list
    gtk_container_foreach(GTK_CONTAINER(g_conv_state->box_msgs),
                          (GtkCallback)gtk_widget_destroy, NULL);

    // Re-parse and display all messages
    std::vector<std::pair<std::string, std::string>> msgs;
    pos = 0;
    while ((pos = resp.find("\"senderId\"", pos)) != std::string::npos) {
      size_t sStart = resp.find('"', pos + 10);
      size_t sEnd = resp.find('"', sStart + 1);
      std::string sid = resp.substr(sStart + 1, sEnd - sStart - 1);
      size_t cPos = resp.find("\"content\"", pos);
      if (cPos == std::string::npos)
        break;
      size_t cStart = resp.find('"', cPos + 9);
      size_t cEnd = resp.find('"', cStart + 1);
      std::string cont = resp.substr(cStart + 1, cEnd - cStart - 1);
      size_t np = 0;
      while ((np = cont.find("\\n", np)) != std::string::npos) {
        cont.replace(np, 2, "\n");
        np++;
      }
      msgs.emplace_back(sid, cont);
      pos = cEnd + 1;
    }

    // Rebuild UI with all messages
    for (auto &m : msgs) {
      bool isMine = (m.first != g_conv_state->recipientId);
      add_chat_bubble(g_conv_state->box_msgs, m.second, isMine,
                      isMine ? "You" : g_conv_state->recipientLabel);
    }
    gtk_widget_show_all(g_conv_state->box_msgs);

    // Scroll to bottom
    GtkAdjustment *adj =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(
            gtk_widget_get_parent(g_conv_state->box_msgs)));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
  }

  return TRUE; // Keep the timer running
}

// Helper to add a chat bubble (reused for history and new messages)
static void add_chat_bubble(GtkWidget *box, const std::string &content,
                            bool isMine, const std::string &senderLabel) {
  std::string bubbleText = (isMine ? "You: " : senderLabel + ": ") + content;

  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *bubble = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(bubble), GTK_SHADOW_OUT);
  GtkWidget *lbl = gtk_label_new(bubbleText.c_str());
  gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
  gtk_container_add(GTK_CONTAINER(bubble), lbl);

  GtkWidget *spacer = gtk_label_new("");
  if (isMine) {
    gtk_box_pack_start(GTK_BOX(row), spacer, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), bubble, FALSE, FALSE, 0);
    gtk_widget_set_halign(bubble, GTK_ALIGN_END);
  } else {
    gtk_box_pack_start(GTK_BOX(row), bubble, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), spacer, TRUE, TRUE, 0);
    gtk_widget_set_halign(bubble, GTK_ALIGN_START);
  }

  gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 4);
  gtk_widget_show_all(row);
}

// Callback for the new "Send" button inside the conversation dialog
static void on_conversation_send_clicked(GtkWidget *widget, gpointer data) {
  ConversationContext *ctx = (ConversationContext *)data;
  const char *text = gtk_entry_get_text(GTK_ENTRY(ctx->entry));
  if (!text || strlen(text) == 0)
    return;

  std::string msg = text;
  // Send to server
  std::string json =
      "{\"messageType\":\"SEND_MESSAGE_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"recipientId\":\"" + ctx->recipientId +
      "\", \"messageContent\":\"" + escape_json(msg) + "\"}}";

  // In a real async UI, we shouldn't block, but here we reuse the simple
  // blocking flow or just fire and forget if looking for responsiveness. To be
  // safe and show result, we wait briefly.
  if (sendMessage(json)) {
    // Assume success for UI responsiveness or wait?
    // Let's do a quick wait to ensure it went through.
    waitForResponse(500);

    // Append to UI
    add_chat_bubble(ctx->box_msgs, msg, true, "You");

    // Clear entry
    gtk_entry_set_text(GTK_ENTRY(ctx->entry), "");
  } else {
    GtkWidget *err =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, "Failed to send");
    gtk_dialog_run(GTK_DIALOG(err));
    gtk_widget_destroy(err);
  }
}

// Conversation popup handler (C-style callback)
static void on_open_conversation_clicked(GtkWidget * /*widget*/,
                                         gpointer /*data*/) {
  // Get selected user ID and label text
  const char *recipient =
      gtk_combo_box_get_active_id(GTK_COMBO_BOX(entry_chat_user));
  if (!recipient)
    return;
  std::string recipientId = recipient;
  gchar *selText =
      gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(entry_chat_user));
  std::string recipientLabel = selText ? selText : "Contact";
  if (selText)
    g_free(selText);

  // Request chat history
  std::string req = std::string("{\"messageType\":\"GET_CHAT_HISTORY_"
                                "REQUEST\", \"sessionToken\":\"") +
                    sessionToken + "\", \"payload\":{\"recipientId\":\"" +
                    recipientId + "\"}}";
  if (!sendMessage(req))
    return;
  std::string resp = waitForResponse(2000);

  // Parse messages (senderId, content)
  std::vector<std::pair<std::string, std::string>> msgs;
  size_t p = 0;
  while ((p = resp.find("\"senderId\"", p)) != std::string::npos) {
    size_t sStart = resp.find('"', p + 10);
    size_t sEnd = resp.find('"', sStart + 1);
    std::string sid = resp.substr(sStart + 1, sEnd - sStart - 1);
    size_t cPos = resp.find("\"content\"", p);
    if (cPos == std::string::npos)
      break;
    size_t cStart = resp.find('"', cPos + 9);
    size_t cEnd = resp.find('"', cStart + 1);
    std::string cont = resp.substr(cStart + 1, cEnd - cStart - 1);
    size_t np = 0;
    while ((np = cont.find("\\n", np)) != std::string::npos) {
      cont.replace(np, 2, "\n");
      np++;
    }
    msgs.emplace_back(sid, cont);
    p = cEnd + 1;
  }

  // Build conversation window
  GtkWidget *conv = gtk_dialog_new_with_buttons(
      "Conversation", GTK_WINDOW(window), GTK_DIALOG_MODAL, "Close",
      GTK_RESPONSE_CLOSE, NULL);
  gtk_window_set_default_size(GTK_WINDOW(conv), 500, 400);

  GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(conv));
  GtkWidget *vbox_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(area), vbox_main);

  // 1. Chat History Area
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(scroll, -1,
                              300); // Set minimum height for chat area
  gtk_box_pack_start(GTK_BOX(vbox_main), scroll, TRUE, TRUE, 0);

  GtkWidget *box_msgs = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add(GTK_CONTAINER(scroll), box_msgs);

  for (auto &m : msgs) {
    bool isMine = (m.first != recipientId);
    add_chat_bubble(box_msgs, m.second, isMine,
                    isMine ? "You" : recipientLabel);
  }

  // 2. Input Area
  GtkWidget *hbox_input = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox_input), 5);
  gtk_box_pack_start(GTK_BOX(vbox_main), hbox_input, FALSE, FALSE, 0);

  GtkWidget *entry_msg = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry_msg), "Type a message...");
  gtk_box_pack_start(GTK_BOX(hbox_input), entry_msg, TRUE, TRUE, 0);

  GtkWidget *btn_send = gtk_button_new_with_label("Send");
  gtk_box_pack_start(GTK_BOX(hbox_input), btn_send, FALSE, FALSE, 0);

  // Setup context for callback
  ConversationContext ctx;
  ctx.box_msgs = box_msgs;
  ctx.entry = entry_msg;
  ctx.recipientId = recipientId;
  ctx.recipientLabel = recipientLabel;

  g_signal_connect(btn_send, "clicked",
                   G_CALLBACK(on_conversation_send_clicked), &ctx);
  g_signal_connect(entry_msg, "activate",
                   G_CALLBACK(on_conversation_send_clicked),
                   &ctx); // Enter key to send

  // Setup auto-refresh state
  if (g_conv_state) {
    if (g_conv_state->timeout_id > 0)
      g_source_remove(g_conv_state->timeout_id);
    delete g_conv_state;
  }
  g_conv_state = new ConversationState;
  g_conv_state->dialog = conv;
  g_conv_state->box_msgs = box_msgs;
  g_conv_state->recipientId = recipientId;
  g_conv_state->recipientLabel = recipientLabel;
  g_conv_state->messageCount = (int)msgs.size();
  // Start auto-refresh every 3 seconds (3000 ms)
  g_conv_state->timeout_id = g_timeout_add(3000, refresh_conversation_messages, NULL);

  gtk_widget_show_all(conv);
  gtk_dialog_run(GTK_DIALOG(conv));

  // Cleanup on close
  if (g_conv_state->timeout_id > 0)
    g_source_remove(g_conv_state->timeout_id);
  delete g_conv_state;
  g_conv_state = nullptr;

  gtk_widget_destroy(conv);
}

static std::vector<GameItem> parse_game_list(const std::string &resp) {
  std::vector<GameItem> out;
  std::regex re_game(
      "\\\"gameId\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"[^}]*\\\"gameType\\\"\\s*:"
      "\\s*"
      "\\\"([^\\\"]+)\\\"[^}]*\\\"title\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"[^}]*"
      "\\\"level\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
  auto it = std::sregex_iterator(resp.begin(), resp.end(), re_game);
  auto end = std::sregex_iterator();
  for (; it != end; ++it) {
    GameItem g;
    g.id = (*it)[1];
    g.type = (*it)[2];
    g.title = (*it)[3];
    g.level = (*it)[4];
    out.push_back(g);
  }
  return out;
}

static std::vector<std::pair<std::string, std::string>>
parse_pairs(const std::string &gameData, const std::string &leftKey,
            const std::string &rightKey) {
  // Parse simple pair arrays in the server payloads (word/sentence/picture
  // match)
  std::vector<std::pair<std::string, std::string>> pairs;
  const std::string pairsKey = "\"pairs\"";
  const std::string lKey = "\"" + leftKey + "\"";
  const std::string rKey = "\"" + rightKey + "\"";

  size_t pos = gameData.find(pairsKey);
  if (pos == std::string::npos)
    return pairs;
  size_t end = gameData.find(']', pos);

  while ((pos = gameData.find(lKey, pos)) != std::string::npos) {
    if (end != std::string::npos && pos > end)
      break;
    size_t lStart = gameData.find('"', pos + lKey.size());
    size_t lEnd = gameData.find('"', lStart + 1);
    std::string left =
        (lStart != std::string::npos && lEnd != std::string::npos)
            ? gameData.substr(lStart + 1, lEnd - lStart - 1)
            : "";

    size_t rPos = gameData.find(rKey, lEnd);
    if (rPos == std::string::npos)
      break;
    size_t rStart = gameData.find('"', rPos + rKey.size());
    size_t rEnd = gameData.find('"', rStart + 1);
    std::string right =
        (rStart != std::string::npos && rEnd != std::string::npos)
            ? gameData.substr(rStart + 1, rEnd - rStart - 1)
            : "";

    pairs.emplace_back(left, right);
    pos = rEnd;
  }
  return pairs;
}

void show_game_dialog() {
  // 1) Lấy danh sách game
  std::string listReq =
      "{\"messageType\":\"GET_GAME_LIST_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"gameType\":\"all\",\"level\":\"" +
      currentLevel + "\"}}";
  if (!sendMessage(listReq))
    return;
  std::string listResp = waitForResponse(3000);
  auto games = parse_game_list(listResp);

  if (games.empty()) {
    GtkWidget *msg =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                               GTK_BUTTONS_OK, "Chưa có game cho level này.");
    gtk_dialog_run(GTK_DIALOG(msg));
    gtk_widget_destroy(msg);
    return;
  }

  // 2) Dialog chọn game
  GtkWidget *pick = gtk_dialog_new_with_buttons(
      "Chọn game", GTK_WINDOW(window), GTK_DIALOG_MODAL, "Đóng",
      GTK_RESPONSE_CLOSE, "Bắt đầu", GTK_RESPONSE_OK, NULL);
  GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(pick));
  GtkWidget *combo = gtk_combo_box_text_new();
  for (auto &g : games) {
    std::string label = g.title + " [" + g.type + "]";
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), g.id.c_str(),
                              label.c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
  gtk_box_pack_start(GTK_BOX(area), gtk_label_new("Chọn game để chơi"), FALSE,
                     FALSE, 8);
  gtk_box_pack_start(GTK_BOX(area), combo, FALSE, FALSE, 8);
  gtk_widget_show_all(pick);
  int resp = gtk_dialog_run(GTK_DIALOG(pick));
  if (resp != GTK_RESPONSE_OK) {
    gtk_widget_destroy(pick);
    return;
  }
  const char *selId = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
  std::string gameId = selId ? selId : games.front().id;
  gtk_widget_destroy(pick);

  // 3) Bắt đầu game
  std::string startReq =
      "{\"messageType\":\"START_GAME_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"gameId\":\"" + gameId + "\"}}";
  if (!sendMessage(startReq))
    return;
  std::string startResp = waitForResponse(4000);
  std::regex re_status("\\\"status\\\"\\s*:\\s*\\\"success\\\"");
  if (!std::regex_search(startResp, re_status)) {
    GtkWidget *err =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, "Không bắt đầu được game.");
    gtk_dialog_run(GTK_DIALOG(err));
    gtk_widget_destroy(err);
    return;
  }

  // Parse game data
  auto val = [&](const std::string &key) -> std::string {
    std::regex re("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch m;
    if (std::regex_search(startResp, m, re) && m.size() > 1)
      return m.str(1);
    return "";
  };
  std::string gameType = val("gameType");
  std::string gameTitle = val("title");
  std::string gameSessionId = val("gameSessionId");
  std::string timeLimit = val("timeLimit");
  std::string gameData = startResp; // reuse whole payload for pair parsing

  // 4) Giao diện chơi game
  GtkWidget *play = gtk_dialog_new_with_buttons(
      gameTitle.c_str(), GTK_WINDOW(window), GTK_DIALOG_MODAL, "Hủy",
      GTK_RESPONSE_CLOSE, "Nộp", GTK_RESPONSE_OK, NULL);
  gtk_window_set_default_size(GTK_WINDOW(play), 520, 480);
  GtkWidget *pa = gtk_dialog_get_content_area(GTK_DIALOG(play));
  GtkWidget *info = gtk_label_new(
      ("Thời gian: " + timeLimit + "s | Loại: " + gameType).c_str());
  gtk_box_pack_start(GTK_BOX(pa), info, FALSE, FALSE, 6);

  GtkWidget *textArea = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(textArea), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textArea), GTK_WRAP_WORD_CHAR);
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_size_request(scroll, -1, 260);
  gtk_container_add(GTK_CONTAINER(scroll), textArea);
  gtk_box_pack_start(GTK_BOX(pa), scroll, TRUE, TRUE, 6);

  GtkWidget *lblGuide =
      gtk_label_new("Nhập ghép cặp theo định dạng 1-3,2-1 ...");
  gtk_box_pack_start(GTK_BOX(pa), lblGuide, FALSE, FALSE, 4);
  GtkWidget *entryMatches = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(pa), entryMatches, FALSE, FALSE, 4);

  // Build content per type
  std::stringstream display;
  std::vector<std::pair<std::string, std::string>> pairs;
  if (gameType == "word_match") {
    pairs = parse_pairs(gameData, "left", "right");
    display << "Cột trái (English) vs cột phải (Vietnamese):\n\n";
    for (size_t i = 0; i < pairs.size(); ++i) {
      display << (i + 1) << ". " << pairs[i].first << "    |    "
              << pairs[i].second << "\n";
    }
  } else if (gameType == "sentence_match") {
    pairs = parse_pairs(gameData, "left", "right");
    display << "Ghép câu hỏi với trả lời:\n\n";
    display << "Câu hỏi:\n";
    for (size_t i = 0; i < pairs.size(); ++i)
      display << "  " << (i + 1) << ") " << pairs[i].first << "\n";
    display << "\nCâu trả lời:\n";
    for (size_t i = 0; i < pairs.size(); ++i)
      display << "  " << (i + 1) << ") " << pairs[i].second << "\n";
  } else if (gameType == "picture_match") {
    pairs = parse_pairs(gameData, "word", "imageUrl");
    display << "Ghép từ với hình ảnh (URL minh họa):\n\n";
    for (size_t i = 0; i < pairs.size(); ++i) {
      display << (i + 1) << ". " << pairs[i].first << " -> " << pairs[i].second
              << "\n";
    }
  } else {
    display << "Loại game chưa hỗ trợ.";
  }

  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textArea));
  gtk_text_buffer_set_text(buf, display.str().c_str(), -1);

  gtk_widget_show_all(play);
  int presp = gtk_dialog_run(GTK_DIALOG(play));
  if (presp != GTK_RESPONSE_OK) {
    gtk_widget_destroy(play);
    return;
  }

  const char *matchStr = gtk_entry_get_text(GTK_ENTRY(entryMatches));
  std::string matchesInput = matchStr ? matchStr : "";

  // Build matches JSON
  std::stringstream matchesJson;
  matchesJson << "[";
  std::istringstream iss(matchesInput);
  std::string token;
  bool first = true;
  while (std::getline(iss, token, ',')) {
    size_t dash = token.find('-');
    if (dash == std::string::npos)
      continue;
    int li = std::max(0, std::stoi(token.substr(0, dash)) - 1);
    int ri = std::max(0, std::stoi(token.substr(dash + 1)) - 1);
    if (li < (int)pairs.size() && ri < (int)pairs.size()) {
      if (!first)
        matchesJson << ",";
      first = false;
      matchesJson << "{\"" << (gameType == "picture_match" ? "word" : "left")
                  << "\":\"" << escape_json(pairs[li].first) << "\",\""
                  << (gameType == "picture_match" ? "imageUrl" : "right")
                  << "\":\"" << escape_json(pairs[ri].second) << "\"}";
    }
  }
  matchesJson << "]";

  gtk_widget_destroy(play);

  // 5) Submit result
  std::string submitReq = "{\"messageType\":\"SUBMIT_GAME_RESULT_REQUEST\","
                          " \"sessionToken\":\"" +
                          sessionToken +
                          "\", \"payload\":{\"gameSessionId\":\"" +
                          gameSessionId + "\",\"gameId\":\"" + gameId +
                          "\",\"matches\":" + matchesJson.str() + "}}";
  if (!sendMessage(submitReq))
    return;
  std::string submitResp = waitForResponse(4000);

  std::string score = "?", maxScore = "?", grade = "?", percentage = "?";
  auto pickVal = [&](const std::string &key) {
    std::regex r("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch m;
    if (std::regex_search(submitResp, m, r) && m.size() > 1)
      return m.str(1);
    return std::string("?");
  };
  score = pickVal("score");
  maxScore = pickVal("maxScore");
  grade = pickVal("grade");
  percentage = pickVal("percentage");

  std::string resultMsg = "Điểm: " + score + "/" + maxScore + " (" +
                          percentage + "%)\nXếp loại: " + grade;
  GtkWidget *res =
      gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                             GTK_BUTTONS_OK, "%s", resultMsg.c_str());
  gtk_dialog_run(GTK_DIALOG(res));
  gtk_widget_destroy(res);
}

static void on_level_item_clicked(GtkWidget *widget, gpointer data) {
  const char *level = (const char *)data;
  std::string json =
      "{\"messageType\":\"SET_LEVEL_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"level\":\"" + std::string(level) +
      "\"}}";

  if (sendMessage(json)) {
    std::string response = waitForResponse(3000);
    if (response.find("success") != std::string::npos) {
      currentLevel = level;
      GtkWidget *msg = gtk_message_dialog_new(
          NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
          "Đã chuyển sang cấp độ: %s", level);
      gtk_dialog_run(GTK_DIALOG(msg));
      gtk_widget_destroy(msg);
      GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
      if (gtk_widget_is_toplevel(toplevel))
        gtk_widget_destroy(toplevel);
    }
  }
}

void show_level_dialog() {
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Cài đặt trình độ", GTK_WINDOW(window), GTK_DIALOG_MODAL, "Đóng",
      GTK_RESPONSE_CLOSE, NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content), 20);

  GtkWidget *lbl = gtk_label_new(NULL);
  gtk_label_set_markup(
      GTK_LABEL(lbl),
      ("Trình độ hiện tại: <b>" + currentLevel + "</b>").c_str());
  gtk_box_pack_start(GTK_BOX(content), lbl, FALSE, FALSE, 20);

  const char *levels[] = {"beginner", "intermediate", "advanced"};
  const char *labels[] = {"🌱 Người mới (Beginner)",
                          "📘 Trung cấp (Intermediate)",
                          "🎓 Nâng cao (Advanced)"};
  for (int i = 0; i < 3; i++) {
    GtkWidget *btn = gtk_button_new_with_label(labels[i]);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_level_item_clicked),
                     (gpointer)levels[i]);
    gtk_box_pack_start(GTK_BOX(content), btn, FALSE, FALSE, 5);
  }
  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

// =========================================================
// VOICE CALL DIALOG
// =========================================================

static void on_voice_call_button_clicked(GtkWidget *widget, gpointer data) {
  std::string *receiverId = static_cast<std::string *>(data);
  if (!receiverId || receiverId->empty()) {
    GtkWidget *error =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, "Invalid contact selected");
    gtk_dialog_run(GTK_DIALOG(error));
    gtk_widget_destroy(error);
    return;
  }

  // Initiate call
  std::string callReq =
      "{\"messageType\":\"VOICE_CALL_INITIATE_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{\"receiverId\":\"" + *receiverId +
      "\", \"audioSource\":\"microphone\"}}";

  GtkWidget *calling =
      gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                             GTK_BUTTONS_CANCEL, "Calling... Please wait");
  gtk_widget_show_now(calling);
  while (gtk_events_pending())
    gtk_main_iteration();

  if (!sendMessage(callReq)) {
    gtk_widget_destroy(calling);
    GtkWidget *error =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, "Failed to initiate call");
    gtk_dialog_run(GTK_DIALOG(error));
    gtk_widget_destroy(error);
    return;
  }

  std::string response = waitForResponse(5000);
  gtk_widget_destroy(calling);

  if (response.find("\"status\":\"success\"") != std::string::npos) {
    // Extract call ID
    size_t idStart = response.find("\"callId\":\"");
    std::string callId = "";
    if (idStart != std::string::npos) {
      idStart += 10;
      size_t idEnd = response.find("\"", idStart);
      callId = response.substr(idStart, idEnd - idStart);
    }

    GtkWidget *callDialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "Call initiated!\n\nCall ID: %s\n\n"
        "(Simulated voice call - no actual audio)\n"
        "Click Close to end the call.",
        callId.c_str());
    gtk_dialog_run(GTK_DIALOG(callDialog));
    gtk_widget_destroy(callDialog);

    // End call
    if (!callId.empty()) {
      std::string endReq =
          "{\"messageType\":\"VOICE_CALL_END_REQUEST\", \"sessionToken\":\"" +
          sessionToken + "\", \"payload\":{\"callId\":\"" + callId + "\"}}";
      sendMessage(endReq);
      waitForResponse(2000);
    }
  } else {
    std::string errorMsg = "Failed to start call";
    size_t msgStart = response.find("\"message\":\"");
    if (msgStart != std::string::npos) {
      msgStart += 11;
      size_t msgEnd = response.find("\"", msgStart);
      errorMsg = response.substr(msgStart, msgEnd - msgStart);
    }
    GtkWidget *error = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s",
        errorMsg.c_str());
    gtk_dialog_run(GTK_DIALOG(error));
    gtk_widget_destroy(error);
  }
}

void show_voice_call_dialog() {
  // Get online contacts
  std::string jsonRequest =
      "{\"messageType\":\"GET_CONTACT_LIST_REQUEST\", \"sessionToken\":\"" +
      sessionToken + "\", \"payload\":{}}";

  GtkWidget *loading =
      gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                             GTK_BUTTONS_NONE, "Loading contacts...");
  gtk_widget_show_now(loading);
  while (gtk_events_pending())
    gtk_main_iteration();

  if (!sendMessage(jsonRequest)) {
    gtk_widget_destroy(loading);
    GtkWidget *error =
        gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, "Failed to load contacts");
    gtk_dialog_run(GTK_DIALOG(error));
    gtk_widget_destroy(error);
    return;
  }

  std::string response = waitForResponse(3000);
  gtk_widget_destroy(loading);

  // Parse online contacts
  std::vector<std::pair<std::string, std::string>> onlineContacts;
  size_t pos = 0;
  while ((pos = response.find("\"userId\"", pos)) != std::string::npos) {
    size_t idStart = response.find("\"", pos + 9);
    size_t idEnd = response.find("\"", idStart + 1);
    std::string userId = response.substr(idStart + 1, idEnd - idStart - 1);

    // Check if online
    size_t statusPos = response.find("\"status\"", pos);
    bool isOnline = false;
    if (statusPos != std::string::npos && statusPos < pos + 300) {
      size_t statusStart = response.find("\"", statusPos + 9);
      size_t statusEnd = response.find("\"", statusStart + 1);
      std::string status =
          response.substr(statusStart + 1, statusEnd - statusStart - 1);
      isOnline = (status == "online");
    }

    if (isOnline) {
      size_t namePos = response.find("\"fullName\"", pos);
      if (namePos == std::string::npos)
        namePos = response.find("\"fullname\"", pos);
      if (namePos != std::string::npos && namePos < pos + 300) {
        size_t nameStart = response.find("\"", namePos + 11);
        size_t nameEnd = response.find("\"", nameStart + 1);
        std::string name =
            response.substr(nameStart + 1, nameEnd - nameStart - 1);
        onlineContacts.push_back({userId, name});
      }
    }
    pos = idEnd;
  }

  // Create dialog
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Voice Call", GTK_WINDOW(window), GTK_DIALOG_MODAL, "Close",
      GTK_RESPONSE_CLOSE, NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 350, 400);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(content), scrolled, TRUE, TRUE, 5);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(scrolled), vbox);

  GtkWidget *title = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(title),
                       "<b>Online Contacts - Click to Call</b>");
  gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 10);

  if (onlineContacts.empty()) {
    GtkWidget *noContacts =
        gtk_label_new("No online contacts available for voice call.");
    gtk_box_pack_start(GTK_BOX(vbox), noContacts, FALSE, FALSE, 20);
  } else {
    // Store user IDs for callbacks (static to persist)
    static std::vector<std::string> storedIds;
    storedIds.clear();
    storedIds.reserve(onlineContacts.size());

    for (const auto &contact : onlineContacts) {
      storedIds.push_back(contact.first);
      std::string label = "Call " + contact.second;
      GtkWidget *btn = gtk_button_new_with_label(label.c_str());
      g_signal_connect(btn, "clicked", G_CALLBACK(on_voice_call_button_clicked),
                       &storedIds.back());
      gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 5);
    }
  }

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

void on_menu_btn_clicked(GtkWidget *widget, gpointer data) {
  int choice = GPOINTER_TO_INT(data);
  switch (choice) {
  case 0:
    show_level_dialog();
    break;
  case 1:
    show_lessons_dialog();
    break;
  case 2:
    show_test_dialog();
    break;
  case 3:
    show_chat_dialog();
    break;
  case 4:
    show_game_dialog();
    break;
  case 5:
    show_voice_call_dialog();
    break;
  case 6:
    gtk_main_quit();
    break;
  }
}

void show_main_menu() {
  if (vbox_login != NULL) {
    gtk_widget_destroy(vbox_login);
    vbox_login = NULL;
  }
  GtkWidget *vbox_menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(window), vbox_menu);
  gtk_container_set_border_width(GTK_CONTAINER(window), 20);

  GtkWidget *lbl = gtk_label_new(NULL);
  gtk_label_set_markup(
      GTK_LABEL(lbl),
      "<span size='x-large' weight='bold'>English Learning App</span>");
  gtk_box_pack_start(GTK_BOX(vbox_menu), lbl, FALSE, FALSE, 10);

  const char *buttons[] = {"1. Chọn cấp độ", "2. Học bài", "3. Làm bài thi",
                           "4. Chat",        "5. Game",    "6. Voice Call", "Thoát"};
  for (int i = 0; i < 7; i++) {
    GtkWidget *btn = gtk_button_new_with_label(buttons[i]);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_menu_btn_clicked),
                     GINT_TO_POINTER(i));
    gtk_box_pack_start(GTK_BOX(vbox_menu), btn, FALSE, FALSE, 5);
  }
  gtk_widget_show_all(window);
}

static void on_login_clicked(GtkWidget *widget, gpointer data) {
  const char *email = gtk_entry_get_text(GTK_ENTRY(entry_email));
  const char *pass = gtk_entry_get_text(GTK_ENTRY(entry_password));
  gtk_label_set_text(GTK_LABEL(lbl_status), "Đang gửi dữ liệu...");
  while (gtk_events_pending())
    gtk_main_iteration();

  std::string jsonRequest =
      "{\"messageType\":\"LOGIN_REQUEST\", \"payload\":{\"email\":\"" +
      std::string(email) + "\", \"password\":\"" + std::string(pass) + "\"}}";

  if (sendMessage(jsonRequest)) {
    std::string response = waitForResponse(3000);
    if (response.find("success") != std::string::npos) {
      sessionToken = "";
      std::regex re_token("\"sessionToken\"\\s*:\\s*\"([^\"]+)\"");
      std::smatch match;
      if (std::regex_search(response, match, re_token) && match.size() > 1)
        sessionToken = match.str(1);

      std::regex re_level("\"level\"\\s*:\\s*\"([^\"]+)\"");
      if (std::regex_search(response, match, re_level) && match.size() > 1)
        currentLevel = match.str(1);

      show_main_menu();
    } else {
      gtk_label_set_text(GTK_LABEL(lbl_status), "Sai mật khẩu!");
    }
  } else {
    gtk_label_set_text(GTK_LABEL(lbl_status), "Lỗi kết nối!");
  }
}

int main(int argc, char *argv[]) {
  if (!connectToServer("127.0.0.1", 8888))
    return 1;
  std::thread recvThread(receiveThreadFunc);
  recvThread.detach();

  gtk_init(&argc, &argv);
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "English App");
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 500);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  vbox_login = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(window), vbox_login);
  gtk_container_set_border_width(GTK_CONTAINER(window), 40);

  gtk_box_pack_start(GTK_BOX(vbox_login), gtk_label_new("Email:"), FALSE, FALSE,
                     0);
  entry_email = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(vbox_login), entry_email, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox_login), gtk_label_new("Password:"), FALSE,
                     FALSE, 0);
  entry_password = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
  gtk_box_pack_start(GTK_BOX(vbox_login), entry_password, FALSE, FALSE, 0);

  GtkWidget *btn_login = gtk_button_new_with_label("Đăng nhập");
  g_signal_connect(btn_login, "clicked", G_CALLBACK(on_login_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox_login), btn_login, FALSE, FALSE, 20);

  lbl_status = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(vbox_login), lbl_status, FALSE, FALSE, 0);

  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}