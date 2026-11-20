package main

import (
	"archive/zip"
	"bytes"
	"crypto/sha256"
	"debug/elf"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

type bundle struct {
	Name  string
	Data  []byte
	Files []string
}

type library struct {
	Name    string   `json:"name"`
	SHA256  string   `json:"sha256"`
	Size    int      `json:"size_bytes"`
	Exports []string `json:"dynamic_exports,omitempty"`
}

var frameworkNames = []string{"libAppFramework.A.so", "libCoreFramework.A.so"}
var hmiBuildPattern = regexp.MustCompile(`(?m)^(NBTevo_ID5_[A-Za-z0-9_.-]+ ID5_[A-Za-z0-9_.-]+ Build [0-9]+)[\r ]*$`)

func libraryInfo(name string, raw []byte) (library, error) {
	file, err := elf.NewFile(bytes.NewReader(raw))
	if err != nil {
		return library{}, fmt.Errorf("%s is not a readable ELF library", name)
	}
	defer file.Close()
	if file.Class != elf.ELFCLASS32 || file.Machine != elf.EM_ARM || file.Data != elf.ELFDATA2LSB || file.Type != elf.ET_DYN {
		return library{}, fmt.Errorf("%s is not a 32-bit ARM shared library", name)
	}
	info := library{Name: name, SHA256: fmt.Sprintf("%x", sha256.Sum256(raw)), Size: len(raw)}
	symbols, err := file.DynamicSymbols()
	if err != nil && !errors.Is(err, elf.ErrNoSymbols) {
		return library{}, fmt.Errorf("could not read %s export table", name)
	}
	seen := map[string]bool{}
	for _, sym := range symbols {
		if sym.Name != "" && sym.Section != elf.SHN_UNDEF && !seen[sym.Name] {
			info.Exports = append(info.Exports, sym.Name)
			seen[sym.Name] = true
		}
	}
	sort.Strings(info.Exports)
	return info, nil
}

var displayPattern = regexp.MustCompile(`(?i)resolution:\s*x\s*=\s*([0-9]{2,4}),\s*y\s*=\s*([0-9]{2,4})\b`)

func collect(r remoteRunner, screenVersion string, includeLibraries bool) (bundle, error) {
	system, err := r.run("uname -a", nil, 512)
	fields := strings.Fields(string(system))
	if err != nil || len(fields) < 4 || fields[0] != "QNX" || fields[len(fields)-1] != "armle" {
		return bundle{}, errors.New("the SSH endpoint did not identify as an ARM QNX head unit")
	}
	node := "unknown"
	firmwarePath := "/opt/sys/etc/nbt_version.txt"
	hmiRoots := []string{"/opt/hmi/ID5"}
	screenCommand := "/opt/sys/bin/LayerManagerControl get screen 0"
	if strings.Contains(fields[1], "jacinto") {
		node = "hu-jacinto"
		firmwarePath = "/etc/nbt_version.txt"
		hmiRoots = []string{"/net/hu-omap/fs/sda0/repository/istep/opt/hmi/ID5", "/net/hu-omap/fs/sda0/opt/hmi/ID5"}
		screenCommand = "on -f hu-omap sh -c 'export LD_LIBRARY_PATH=/opt/sys/lib:/usr/lib:/lib; " + screenCommand + "'"
	} else if strings.Contains(fields[1], "omap") {
		node = "hu-omap"
	}
	rawVersion, _ := r.run("cat "+firmwarePath, nil, 1024)
	version := firmwareName(string(rawVersion))
	entered := firmwareName(screenVersion)
	if screenVersion != "" && entered == "" {
		return bundle{}, errors.New("enter the complete NBTevo code from the iDrive screen")
	}
	if entered != "" && version != "" && entered != version {
		return bundle{}, errors.New("the iDrive screen code and SSH firmware code differ")
	}
	if version == "" {
		version = entered
	}
	if version == "" {
		return bundle{}, errors.New("firmware code unavailable over SSH; enter the code shown in iDrive")
	}
	type display struct {
		Width  int `json:"width"`
		Height int `json:"height"`
	}
	report := struct {
		Schema     int       `json:"schema"`
		Firmware   string    `json:"firmware"`
		HMIBuild   string    `json:"hmi_build,omitempty"`
		SSHNode    string    `json:"ssh_node"`
		QNXRelease string    `json:"qnx_release"`
		Display    *display  `json:"display,omitempty"`
		Libraries  []library `json:"framework_libraries"`
	}{Schema: 2, Firmware: version, SSHNode: node, QNXRelease: fields[2]}
	rawScreen, _ := r.run(screenCommand, nil, 32<<10)
	if size := displayPattern.FindSubmatch(rawScreen); len(size) == 3 {
		width, _ := strconv.Atoi(string(size[1]))
		height, _ := strconv.Atoi(string(size[2]))
		if width > 0 && height > 0 {
			report.Display = &display{width, height}
		}
	}

	files := make(map[string][]byte)
	for _, root := range hmiRoots {
		var libs []library
		rawLibs := make(map[string][]byte)
		for _, name := range frameworkNames {
			raw, err := r.run("cat "+root+"/lib/"+name, nil, 4<<20)
			if err != nil {
				break
			}
			info, err := libraryInfo(name, raw)
			if err != nil {
				break
			}
			libs = append(libs, info)
			rawLibs[name] = raw
		}
		if len(libs) != len(frameworkNames) {
			continue
		}
		report.Libraries = libs
		hmi, _ := r.run("cat "+root+"/version.txt", nil, 1024)
		if build := hmiBuildPattern.FindSubmatch(hmi); len(build) > 1 {
			report.HMIBuild = string(build[1])
		}
		if includeLibraries {
			for name, raw := range rawLibs {
				files["frameworks/"+name] = raw
			}
		}
		break
	}
	if len(report.Libraries) == 0 {
		return bundle{}, fmt.Errorf("could not read both ARM framework libraries on %s", node)
	}
	reportJSON, err := json.MarshalIndent(report, "", "  ")
	if err != nil {
		return bundle{}, err
	}
	files["compatibility.json"] = append(reportJSON, '\n')
	var result bundle
	result.Name = "Trio-compatibility-" + version + ".zip"
	for path := range files {
		result.Files = append(result.Files, path)
	}
	sort.Strings(result.Files)
	var output bytes.Buffer
	z := zip.NewWriter(&output)
	for _, path := range result.Files {
		w, err := z.Create(path)
		if err != nil {
			return bundle{}, err
		}
		if _, err = w.Write(files[path]); err != nil {
			return bundle{}, err
		}
	}
	if err = z.Close(); err != nil {
		return bundle{}, err
	}
	result.Data = output.Bytes()
	return result, nil
}
