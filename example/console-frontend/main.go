package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"

	"console-frontend/pb"

	"github.com/charmbracelet/bubbles/textinput"
	"github.com/charmbracelet/bubbles/viewport"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/gorilla/websocket"
	"github.com/hinshun/vt10x"
	"google.golang.org/protobuf/proto"
)

// --- Configuration ---
const DefaultBaseURL = "http://192.168.4.1"

// Global HTTP Client for reuse
var httpClient = &http.Client{
	Timeout: 5 * time.Second,
}

// --- Styles ---
var (
	titleStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FFFDF5")).
			Background(lipgloss.Color("#25A065")).
			Padding(0, 1)

	infoStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FFFDF5")).
			Padding(0, 1)

	headerStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("205")).
			Bold(true)

	statusBarStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FFFDF5")).
			Background(lipgloss.Color("#3C3C3C")).
			Padding(0, 1)

	commandModeStyle = lipgloss.NewStyle().
				Foreground(lipgloss.Color("#000000")).
				Background(lipgloss.Color("#D75F00")).
				Padding(0, 1).
				Bold(true)

	errorStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("196"))

	// Help Window Style
	helpBoxStyle = lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).
			BorderForeground(lipgloss.Color("#874BFD")).
			Padding(1, 2).
			Background(lipgloss.Color("#1e1e1e"))

	// Settings Style
	settingItemStyle  = lipgloss.NewStyle().PaddingLeft(2)
	selectedItemStyle = lipgloss.NewStyle().PaddingLeft(0).Foreground(lipgloss.Color("170")).Bold(true).SetString("> ")
)

// --- Messages ---
type sessionMsg struct {
	token string
}

type initStatusMsg struct {
	mainOn bool
	usbOn  bool
}

type settingsMsg struct {
	settings DeviceSettings
}

type errMsg error

type wsMsg *pb.StatusMessage

// --- Data Structures ---

// DeviceSettings maps to GET/POST /api/setting
type DeviceSettings struct {
	VinCurrentLimit  float64 `json:"vin_current_limit,omitempty"`
	MainCurrentLimit float64 `json:"main_current_limit,omitempty"`
	UsbCurrentLimit  float64 `json:"usb_current_limit,omitempty"`
	BaudRate         int     `json:"baud_rate,omitempty"`
	SensorPeriod     int     `json:"sensor_period,omitempty"`
}

// --- API Client ---
type LoginRequest struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

type LoginResponse struct {
	Token string `json:"token"`
}

type ControlRequest struct {
	Load12vOn    *bool `json:"load_12v_on,omitempty"`
	Load5vOn     *bool `json:"load_5v_on,omitempty"`
	PowerTrigger *bool `json:"power_trigger,omitempty"`
	ResetTrigger *bool `json:"reset_trigger,omitempty"`
}

type ControlStatusResponse struct {
	Load12vOn bool `json:"load_12v_on"`
	Load5vOn  bool `json:"load_5v_on"`
}

func login(baseURL, username, password string) tea.Cmd {
	return func() tea.Msg {
		baseURL = strings.TrimRight(baseURL, "/")
		reqBody, _ := json.Marshal(LoginRequest{Username: username, Password: password})
		resp, err := httpClient.Post(baseURL+"/login", "application/json", bytes.NewBuffer(reqBody))
		if err != nil {
			return errMsg(err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != 200 {
			body, _ := io.ReadAll(resp.Body)
			return errMsg(fmt.Errorf("login failed: %s", string(body)))
		}

		var loginResp LoginResponse
		if err := json.NewDecoder(resp.Body).Decode(&loginResp); err != nil {
			return errMsg(err)
		}
		return sessionMsg{token: loginResp.Token}
	}
}

func fetchControlStatus(baseURL, token string) tea.Cmd {
	return func() tea.Msg {
		baseURL = strings.TrimRight(baseURL, "/")
		req, _ := http.NewRequest("GET", baseURL+"/api/control", nil)
		req.Header.Set("Authorization", "Bearer "+token)

		resp, err := httpClient.Do(req)
		if err != nil {
			return errMsg(err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != 200 {
			return errMsg(fmt.Errorf("failed to fetch status: %d", resp.StatusCode))
		}

		var statusResp ControlStatusResponse
		if err := json.NewDecoder(resp.Body).Decode(&statusResp); err != nil {
			return errMsg(err)
		}

		return initStatusMsg{
			mainOn: statusResp.Load12vOn,
			usbOn:  statusResp.Load5vOn,
		}
	}
}

func fetchDeviceSettings(baseURL, token string) tea.Cmd {
	return func() tea.Msg {
		baseURL = strings.TrimRight(baseURL, "/")
		req, _ := http.NewRequest("GET", baseURL+"/api/setting", nil)
		req.Header.Set("Authorization", "Bearer "+token)

		resp, err := httpClient.Do(req)
		if err != nil {
			return errMsg(err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != 200 {
			return errMsg(fmt.Errorf("failed to fetch settings: %d", resp.StatusCode))
		}

		var s DeviceSettings
		if err := json.NewDecoder(resp.Body).Decode(&s); err != nil {
			return errMsg(err)
		}
		return settingsMsg{settings: s}
	}
}

func saveDeviceSetting(baseURL, token string, setting DeviceSettings) tea.Cmd {
	return func() tea.Msg {
		baseURL = strings.TrimRight(baseURL, "/")
		reqBody, _ := json.Marshal(setting)
		req, _ := http.NewRequest("POST", baseURL+"/api/setting", bytes.NewBuffer(reqBody))
		req.Header.Set("Authorization", "Bearer "+token)
		req.Header.Set("Content-Type", "application/json")

		resp, err := httpClient.Do(req)
		if err != nil {
			return errMsg(err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != 200 {
			return errMsg(fmt.Errorf("save failed: %d", resp.StatusCode))
		}
		// Refresh settings after save
		return fetchDeviceSettings(baseURL, token)()
	}
}

func rebootDevice(baseURL, token string) tea.Cmd {
	return func() tea.Msg {
		baseURL = strings.TrimRight(baseURL, "/")
		req, _ := http.NewRequest("POST", baseURL+"/api/reboot", nil)
		req.Header.Set("Authorization", "Bearer "+token)
		httpClient.Do(req) // Don't wait for response too strictly as device reboots
		return nil
	}
}

func sendControl(baseURL, token string, payload ControlRequest) tea.Cmd {
	return func() tea.Msg {
		baseURL = strings.TrimRight(baseURL, "/")
		reqBody, _ := json.Marshal(payload)
		req, _ := http.NewRequest("POST", baseURL+"/api/control", bytes.NewBuffer(reqBody))
		req.Header.Set("Authorization", "Bearer "+token)
		req.Header.Set("Content-Type", "application/json")

		resp, err := httpClient.Do(req)
		if err != nil {
			return errMsg(err)
		}
		defer resp.Body.Close()

		if resp.StatusCode != 200 {
			body, _ := io.ReadAll(resp.Body)
			return errMsg(fmt.Errorf("control failed (%d): %s", resp.StatusCode, string(body)))
		}

		return nil
	}
}

// --- WebSocket Handling ---
func connectWebSocket(baseURL, token string, sendChan <-chan []byte) tea.Cmd {
	return func() tea.Msg {
		u, err := url.Parse(baseURL)
		if err != nil {
			return errMsg(err)
		}
		scheme := "ws"
		if u.Scheme == "https" {
			scheme = "wss"
		}
		wsURL := fmt.Sprintf("%s://%s/ws?token=%s", scheme, u.Host, token)

		c, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
		if err != nil {
			return errMsg(err)
		}

		go func() {
			for data := range sendChan {
				err := c.WriteMessage(websocket.BinaryMessage, data)
				if err != nil {
					return
				}
			}
			c.Close()
		}()

		go func() {
			defer c.Close()
			for {
				_, message, err := c.ReadMessage()
				if err != nil {
					return
				}
				var statusMsg pb.StatusMessage
				if err := proto.Unmarshal(message, &statusMsg); err == nil {
					program.Send(wsMsg(&statusMsg))
				}
			}
		}()
		return nil
	}
}

var program *tea.Program

// --- Model ---
type state int

const (
	stateLogin state = iota
	stateDashboard
	stateSettings
)

// Setting Item Types
const (
	SetVinLimit = iota
	SetMainLimit
	SetUsbLimit
	SetBaudRate
	SetPeriod
	SetReboot
)

type model struct {
	state       state
	baseURL     string
	token       string
	width       int
	height      int
	err         error

	wsSend chan []byte

	// Login
	serverInput   textinput.Model
	usernameInput textinput.Model
	passwordInput textinput.Model
	focusIndex    int

	// Dashboard
	sensorData      *pb.SensorData
	wifiStatus      *pb.WifiStatus
	swStatus        *pb.LoadSwStatus
	term            vt10x.Terminal
	logViewport     viewport.Model
	awaitingCommand bool
	lastStatusMsg   string

	// Settings
	deviceSettings DeviceSettings
	settingCursor  int
	settingEditing bool            // True if currently typing a value
	settingInput   textinput.Model // Shared input for editing values
}

func initialModel() model {
	s := textinput.New()
	s.Placeholder = "http://192.168.4.1"
	s.SetValue(DefaultBaseURL)
	s.Focus()

	u := textinput.New()
	u.Placeholder = "Username"

	p := textinput.New()
	p.Placeholder = "Password"
	p.EchoMode = textinput.EchoPassword

	vp := viewport.New(80, 24)
	vp.SetContent("Waiting for data...")

	// vt10x 초기화 (Writer는 필요 없으므로 Discard)
	term := vt10x.New(vt10x.WithWriter(io.Discard))
	term.Resize(80, 24)

	si := textinput.New()
	si.CharLimit = 10

	return model{
		state:         stateLogin,
		serverInput:   s,
		usernameInput: u,
		passwordInput: p,
		logViewport:   vp,
		term:          term,
		swStatus:      &pb.LoadSwStatus{Main: false, Usb: false},
		wsSend:        make(chan []byte, 100),
		settingInput:  si,
	}
}

func (m model) Init() tea.Cmd {
	return textinput.Blink
}

func (m model) renderVt10x() string {
	var sb strings.Builder
	rows := m.logViewport.Height
	cols := m.logViewport.Width
	sb.Grow(rows * cols * 4)

	cursor := m.term.Cursor()
	curFG := vt10x.DefaultFG
	curBG := vt10x.DefaultBG

	sb.WriteString("\x1b[0m")

	for y := 0; y < rows; y++ {
		for x := 0; x < cols; x++ {
			cell := m.term.Cell(x, y)

			fg := cell.FG
			bg := cell.BG
			char := cell.Char
			isCursor := (x == cursor.X && y == cursor.Y)

			if fg != curFG || bg != curBG || isCursor {
				sb.WriteString("\x1b[0m")
				if isCursor {
					sb.WriteString("\x1b[7m")
				}
				if fg != vt10x.DefaultFG {
					fmt.Fprintf(&sb, "\x1b[38;5;%dm", fg)
				}
				if bg != vt10x.DefaultBG {
					fmt.Fprintf(&sb, "\x1b[48;5;%dm", bg)
				}
				curFG = fg
				curBG = bg

				if isCursor {
					curFG = vt10x.Color(65535)
					curBG = vt10x.Color(65535)
				}
			}
			sb.WriteRune(char)
		}
		if y < rows-1 {
			sb.WriteString("\x1b[0m\n")
			curFG = vt10x.DefaultFG
			curBG = vt10x.DefaultBG
		}
	}
	return sb.String()
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	var cmd tea.Cmd
	var cmds []tea.Cmd

	switch msg := msg.(type) {
	case tea.KeyMsg:
		if m.state == stateLogin {
			if msg.Type == tea.KeyCtrlC {
				return m, tea.Quit
			}
			switch msg.String() {
			case "tab":
				m.focusIndex = (m.focusIndex + 1) % 3
				m.serverInput.Blur()
				m.usernameInput.Blur()
				m.passwordInput.Blur()
				switch m.focusIndex {
				case 0:
					m.serverInput.Focus()
				case 1:
					m.usernameInput.Focus()
				case 2:
					m.passwordInput.Focus()
				}
				return m, textinput.Blink
			case "enter":
				m.baseURL = m.serverInput.Value()
				if m.baseURL == "" {
					m.baseURL = DefaultBaseURL
				}
				return m, login(m.baseURL, m.usernameInput.Value(), m.passwordInput.Value())
			case "esc":
				return m, tea.Quit
			default:
				if m.focusIndex == 0 {
					m.serverInput, cmd = m.serverInput.Update(msg)
					cmds = append(cmds, cmd)
				} else if m.focusIndex == 1 {
					m.usernameInput, cmd = m.usernameInput.Update(msg)
					cmds = append(cmds, cmd)
				} else {
					m.passwordInput, cmd = m.passwordInput.Update(msg)
					cmds = append(cmds, cmd)
				}
				return m, tea.Batch(cmds...)
			}
		} else if m.state == stateSettings {
			// SETTINGS STATE
			if m.settingEditing {
				switch msg.Type {
				case tea.KeyEnter:
					// Save value
					m.settingEditing = false
					val := m.settingInput.Value()
					var cmd tea.Cmd

					switch m.settingCursor {
					case SetVinLimit:
						v, _ := strconv.ParseFloat(val, 64)
						cmd = saveDeviceSetting(m.baseURL, m.token, DeviceSettings{VinCurrentLimit: v})
					case SetMainLimit:
						v, _ := strconv.ParseFloat(val, 64)
						cmd = saveDeviceSetting(m.baseURL, m.token, DeviceSettings{MainCurrentLimit: v})
					case SetUsbLimit:
						v, _ := strconv.ParseFloat(val, 64)
						cmd = saveDeviceSetting(m.baseURL, m.token, DeviceSettings{UsbCurrentLimit: v})
					case SetBaudRate:
						v, _ := strconv.Atoi(val)
						cmd = saveDeviceSetting(m.baseURL, m.token, DeviceSettings{BaudRate: v})
					case SetPeriod:
						v, _ := strconv.Atoi(val)
						cmd = saveDeviceSetting(m.baseURL, m.token, DeviceSettings{SensorPeriod: v})
					}
					return m, cmd
				case tea.KeyEsc:
					m.settingEditing = false
					return m, nil
				default:
					m.settingInput, cmd = m.settingInput.Update(msg)
					return m, cmd
				}
			} else {
				// Navigation
				switch msg.String() {
				case "q", "esc":
					m.state = stateDashboard
					return m, nil
				case "up":
					if m.settingCursor > 0 {
						m.settingCursor--
					}
				case "down":
					if m.settingCursor < 5 { // items count - 1
						m.settingCursor++
					}
				case "enter":
					if m.settingCursor == SetReboot {
						return m, rebootDevice(m.baseURL, m.token)
					}
					// Start editing
					m.settingEditing = true
					m.settingInput.Focus()
					// Pre-fill value
					switch m.settingCursor {
					case SetVinLimit:
						m.settingInput.SetValue(fmt.Sprintf("%.1f", m.deviceSettings.VinCurrentLimit))
					case SetMainLimit:
						m.settingInput.SetValue(fmt.Sprintf("%.1f", m.deviceSettings.MainCurrentLimit))
					case SetUsbLimit:
						m.settingInput.SetValue(fmt.Sprintf("%.1f", m.deviceSettings.UsbCurrentLimit))
					case SetBaudRate:
						m.settingInput.SetValue(fmt.Sprintf("%d", m.deviceSettings.BaudRate))
					case SetPeriod:
						m.settingInput.SetValue(fmt.Sprintf("%d", m.deviceSettings.SensorPeriod))
					}
					return m, textinput.Blink
				}
			}
		} else if m.state == stateDashboard {
			// DASHBOARD STATE
			keyStr := msg.String()

			if keyStr == "ctrl+a" {
				m.awaitingCommand = !m.awaitingCommand
				if m.awaitingCommand {
					m.lastStatusMsg = "Press '?' for help"
				} else {
					m.lastStatusMsg = ""
				}
				return m, nil
			}

			if m.awaitingCommand {
				m.awaitingCommand = false
				m.lastStatusMsg = ""
				switch keyStr {
				case "m":
					m.lastStatusMsg = "Toggling Main Power..."
					newState := !m.swStatus.Main
					payload := ControlRequest{Load12vOn: &newState}
					return m, sendControl(m.baseURL, m.token, payload)
				case "u":
					m.lastStatusMsg = "Toggling USB Power..."
					newState := !m.swStatus.Usb
					payload := ControlRequest{Load5vOn: &newState}
					return m, sendControl(m.baseURL, m.token, payload)
				case "r":
					m.lastStatusMsg = "Sending Reset Signal..."
					t := true
					payload := ControlRequest{ResetTrigger: &t}
					return m, sendControl(m.baseURL, m.token, payload)
				case "p":
					m.lastStatusMsg = "Sending Power Action..."
					t := true
					payload := ControlRequest{PowerTrigger: &t}
					return m, sendControl(m.baseURL, m.token, payload)
				case "a":
					m.wsSend <- []byte{1}
					return m, nil
				case "o": // Open Settings
					m.state = stateSettings
					return m, fetchDeviceSettings(m.baseURL, m.token)
				case "x": // Quit Program
					return m, tea.Quit
				case "q": // Close Command Window
					m.lastStatusMsg = "Command mode closed."
					return m, nil
				default:
					m.lastStatusMsg = "Command cancelled."
					return m, nil
				}
			}

			// Send to UART
			var data []byte
			switch msg.Type {
			case tea.KeyRunes:
				data = []byte(string(msg.Runes))
			case tea.KeyEnter:
				data = []byte{'\r'}
			case tea.KeySpace:
				data = []byte{' '}
			case tea.KeyBackspace:
				data = []byte{127}
			case tea.KeyTab:
				data = []byte{'\t'}
			case tea.KeyEsc:
				data = []byte{27}
			case tea.KeyUp:
				data = []byte("\x1b[A")
			case tea.KeyDown:
				data = []byte("\x1b[B")
			case tea.KeyRight:
				data = []byte("\x1b[C")
			case tea.KeyLeft:
				data = []byte("\x1b[D")
			case tea.KeyHome:
				data = []byte("\x1b[H")
			case tea.KeyEnd:
				data = []byte("\x1b[F")
			case tea.KeyPgUp:
				data = []byte("\x1b[5~")
			case tea.KeyPgDown:
				data = []byte("\x1b[6~")
			case tea.KeyDelete:
				data = []byte("\x1b[3~")
			case tea.KeyInsert:
				data = []byte("\x1b[2~")
			default:
				if len(keyStr) == 6 && strings.HasPrefix(keyStr, "ctrl+") {
					c := keyStr[5]
					if c >= 'a' && c <= 'z' {
						data = []byte{byte(c - 'a' + 1)}
					} else if c == '@' {
						data = []byte{0}
					} else if c == '[' {
						data = []byte{27}
					} else if c == '\\' {
						data = []byte{28}
					} else if c == ']' {
						data = []byte{29}
					} else if c == '^' {
						data = []byte{30}
					} else if c == '_' {
						data = []byte{31}
					}
				} else if len(msg.Runes) > 0 {
					data = []byte(string(msg.Runes))
				}
			}
			if len(data) > 0 {
				m.wsSend <- data
			}
			return m, nil
		}

	case tea.MouseMsg:
		if m.state == stateDashboard {
			var data []byte
			switch msg.Type {
			case tea.MouseWheelUp:
				data = []byte("\x1b[A")
			case tea.MouseWheelDown:
				data = []byte("\x1b[B")
			}
			if len(data) > 0 {
				m.wsSend <- data
			}
		}

	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		headerHeight := 10
		termW := msg.Width - 2
		termH := msg.Height - headerHeight
		if termH < 5 {
			termH = 5
		}

		m.logViewport.Width = termW
		m.logViewport.Height = termH
		m.term.Resize(termW, termH)

	case sessionMsg:
		m.token = msg.token
		m.state = stateDashboard
		return m, tea.Batch(
			connectWebSocket(m.baseURL, m.token, m.wsSend),
			fetchControlStatus(m.baseURL, m.token),
		)

	case settingsMsg:
		m.deviceSettings = msg.settings
		return m, nil

	case initStatusMsg:
		m.swStatus.Main = msg.mainOn
		m.swStatus.Usb = msg.usbOn
		return m, nil

	case errMsg:
		m.err = msg
		return m, nil

	case wsMsg:
		switch payload := msg.Payload.(type) {
		case *pb.StatusMessage_SensorData:
			m.sensorData = payload.SensorData
		case *pb.StatusMessage_WifiStatus:
			m.wifiStatus = payload.WifiStatus
		case *pb.StatusMessage_SwStatus:
			m.swStatus = payload.SwStatus
		case *pb.StatusMessage_UartData:
			m.term.Write(payload.UartData.Data)
			m.logViewport.SetContent(m.renderVt10x())
		}
	}

	return m, tea.Batch(cmds...)
}

func (m model) View() string {
	if m.err != nil {
		return fmt.Sprintf("\nError: %v\n\nPress q to quit", errorStyle.Render(m.err.Error()))
	}

	if m.state == stateLogin {
		return m.loginView()
	} else if m.state == stateSettings {
		return m.settingsView()
	}

	return m.dashboardView()
}

func (m model) loginView() string {
	var b strings.Builder
	b.WriteString("\n\n")
	b.WriteString(titleStyle.Render(" ODROID Power Mate Login "))
	b.WriteString("\n\n")

	b.WriteString("Server Address:\n")
	b.WriteString(m.serverInput.View())
	b.WriteString("\n\n")

	b.WriteString("Username:\n")
	b.WriteString(m.usernameInput.View())
	b.WriteString("\n\nPassword:\n")
	b.WriteString(m.passwordInput.View())
	b.WriteString("\n\n(Press Tab to switch, Enter to login, Ctrl+C to quit)")
	return lipgloss.Place(m.width, m.height, lipgloss.Center, lipgloss.Center, b.String())
}

func (m model) settingsView() string {
	var s strings.Builder
	s.WriteString(titleStyle.Render(" Device Settings ") + "\n\n")

	items := []struct {
		label string
		value string
	}{
		{"VIN Current Limit (A)", fmt.Sprintf("%.1f", m.deviceSettings.VinCurrentLimit)},
		{"Main Current Limit (A)", fmt.Sprintf("%.1f", m.deviceSettings.MainCurrentLimit)},
		{"USB Current Limit (A)", fmt.Sprintf("%.1f", m.deviceSettings.UsbCurrentLimit)},
		{"UART Baud Rate", fmt.Sprintf("%d", m.deviceSettings.BaudRate)},
		{"Sensor Period (ms)", fmt.Sprintf("%d", m.deviceSettings.SensorPeriod)},
		{"[ Reboot Device ]", ""},
	}

	for i, item := range items {
		label := item.label
		value := item.value

		if i == m.settingCursor {
			s.WriteString(selectedItemStyle.Render(label))
			if m.settingEditing {
				s.WriteString(": " + m.settingInput.View())
			} else {
				s.WriteString(": " + value)
			}
		} else {
			s.WriteString(settingItemStyle.Render(label + ": " + value))
		}
		s.WriteString("\n")
	}

	s.WriteString("\n(Enter to edit, Esc/q to back)")
	return lipgloss.Place(m.width, m.height, lipgloss.Center, lipgloss.Center, s.String())
}

func (m model) dashboardView() string {
	// Base Dashboard
	header := titleStyle.Render(" ODROID Power Mate ")
	if m.wifiStatus != nil {
		ssid := m.wifiStatus.Ssid
		if ssid == "" {
			ssid = "Disconnected"
		}
		header += infoStyle.Render(fmt.Sprintf(" WiFi: %s (%s)", ssid, m.wifiStatus.IpAddress))
	} else {
		header += infoStyle.Render(" WiFi: --")
	}

	var statusText string
	var statusStyle lipgloss.Style
	if m.awaitingCommand {
		statusText = " COMMAND MODE (Ctrl-A) >> Press: M(Main) U(USB) P(Power) R(Reset) O(Settings) A(Ctrl+A) Q(Close) X(Quit)"
		statusStyle = commandModeStyle
	} else {
		statusText = " TERMINAL MODE >> Press Ctrl-A for Commands "
		if m.lastStatusMsg != "" {
			statusText += "| " + m.lastStatusMsg
		}
		statusStyle = statusBarStyle
	}
	bar := statusStyle.Width(m.width).Render(statusText)

	metrics := ""
	if m.sensorData != nil {
		metrics = fmt.Sprintf(
			"USB:  %.2f V / %.2f A / %.2f W\nMAIN: %.2f V / %.2f A / %.2f W\nVIN:  %.2f V / %.2f A / %.2f W\nUptime: %ds",
			m.sensorData.Usb.Voltage, m.sensorData.Usb.Current, m.sensorData.Usb.Power,
			m.sensorData.Main.Voltage, m.sensorData.Main.Current, m.sensorData.Main.Power,
			m.sensorData.Vin.Voltage, m.sensorData.Vin.Current, m.sensorData.Vin.Power,
			m.sensorData.UptimeMs/1000,
		)
	} else {
		metrics = "Waiting for sensor data..."
	}
	metricsBox := lipgloss.NewStyle().Border(lipgloss.RoundedBorder()).Padding(0, 1).Width(m.width/2 - 2).Render(metrics)

	switches := "States:\n"
	mainState := "[OFF]"
	if m.swStatus.Main {
		mainState = "[ON ]"
	}
	usbState := "[OFF]"
	if m.swStatus.Usb {
		usbState = "[ON ]"
	}
	switches += fmt.Sprintf("Main: %s\n", mainState)
	switches += fmt.Sprintf("USB : %s\n", usbState)
	switchBox := lipgloss.NewStyle().Border(lipgloss.RoundedBorder()).Padding(0, 1).Width(m.width/2 - 2).Render(switches)

	topSection := lipgloss.JoinHorizontal(lipgloss.Top, metricsBox, switchBox)

	termView := lipgloss.NewStyle().
		Border(lipgloss.NormalBorder()).
		BorderForeground(lipgloss.Color("240")).
		Width(m.width - 2).
		Height(m.logViewport.Height).
		Render(m.logViewport.View())

	baseView := lipgloss.JoinVertical(lipgloss.Left, header, topSection, bar, termView)

	// --- Help Overlay ---
	if m.awaitingCommand {
		helpText := `
COMMAND MODE (Ctrl+A)

  m : Toggle Main Power
  u : Toggle USB Power
  p : Power Trigger (Long Press)
  r : Reset Trigger
  o : Open Settings
  a : Send 'Ctrl+A'
  q : Close Command Window
  x : Quit Program
		`
		helpWindow := helpBoxStyle.Render(helpText)

		// Overlay help window on top of base view using lipgloss.Place
		return lipgloss.Place(m.width, m.height, lipgloss.Center, lipgloss.Center, helpWindow, lipgloss.WithWhitespaceChars(" "), lipgloss.WithWhitespaceForeground(lipgloss.NoColor{}))
	}

	return baseView
}

func main() {
	program = tea.NewProgram(initialModel(), tea.WithAltScreen(), tea.WithMouseCellMotion())
	if _, err := program.Run(); err != nil {
		fmt.Printf("Error running program: %v\n", err)
		os.Exit(1)
	}
}