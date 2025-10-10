# Git LFS Setup Guide for Nova Perception Engine

This guide explains how to set up Git LFS (Large File Storage) to handle large binary files in this repository.

---

## 🤔 Why Git LFS?

**Problem:** Large binary files (models, DLLs) make the repository huge and slow to clone.

**Currently NOT in Git (due to `.gitignore`):**
- AI Models (~45MB): `models/whisper/ggml-tiny.en-q8_0.bin`, `models/vad/silero_vad.onnx`
- OpenCV (~120MB): `windows_code/third-party/opencv/`
- ONNX Runtime (~15MB): `windows_code/third-party/onnxruntime/`
- Build artifacts: `windows_code/build/`, `*.exe`, `*.dll`

**Solution:** Git LFS stores large files outside the repository and downloads them on-demand.

---

## ✅ Current Recommended Approach (Using `setup.bat`)

**For users cloning the repository:**

```powershell
# Clone the repository
git clone <your-repo-url> PE
cd PE

# Run automated setup to download everything
.\setup.bat
```

**Total download:** ~180MB (models + OpenCV + ONNX Runtime)
**Setup time:** 10-15 minutes

This is the **easiest and most reliable** approach for now.

---

## 🔧 Alternative: Using Git LFS (Advanced)

If you want to commit large files to Git (not recommended for public repos):

### **Step 1: Install Git LFS**

**Windows:**
```powershell
# Using Chocolatey
choco install git-lfs

# Or download from: https://git-lfs.github.com/
```

**Verify installation:**
```powershell
git lfs version
# Output: git-lfs/3.x.x (GitHub; windows amd64; go 1.x.x)
```

### **Step 2: Initialize Git LFS in Repository**

```powershell
cd PE
git lfs install
# Output: Updated Git hooks.
```

### **Step 3: Track Large File Types**

The `.gitattributes` file already contains LFS patterns:
```
*.bin filter=lfs diff=lfs merge=lfs -text
*.onnx filter=lfs diff=lfs merge=lfs -text
*.dll filter=lfs diff=lfs merge=lfs -text
# ... etc
```

This tells Git LFS to handle these file types automatically.

### **Step 4: Migrate Existing Files to LFS (If Already Committed)**

**WARNING:** Only do this if you already committed large files and want to migrate them to LFS.

```powershell
# Migrate specific file types to LFS
git lfs migrate import --include="*.bin,*.onnx,*.dll,*.lib" --everything

# This rewrites Git history to use LFS pointers
```

### **Step 5: Add Files and Commit**

```powershell
# Remove current .gitignore entries for files you want to track with LFS
# Edit .gitignore to remove:
#   models/
#   windows_code/third-party/opencv/
#   windows_code/third-party/onnxruntime/

# Add files
git add models/
git add windows_code/third-party/opencv/
git add windows_code/third-party/onnxruntime/

# Commit
git commit -m "Add large files via Git LFS"

# Push (this will upload to LFS storage)
git push
```

---

## 📊 Git LFS vs setup.bat Comparison

| Approach | Pros | Cons |
|----------|------|------|
| **setup.bat (Current)** | ✅ No LFS setup needed<br>✅ Works everywhere<br>✅ No GitHub LFS quota<br>✅ Easy to understand | ❌ User must run setup.bat<br>❌ Downloads from external sources |
| **Git LFS** | ✅ Files tracked in Git<br>✅ Version controlled<br>✅ Automatic download on clone | ❌ Requires LFS setup<br>❌ GitHub LFS quota (1GB free)<br>❌ More complex<br>❌ ~180MB counts against quota |

---

## 🎯 Recommended Strategy

### **For Public/Open Source Projects:**
✅ **Use `setup.bat`** (current approach)
- Keep large files in `.gitignore`
- Download from official sources (HuggingFace, GitHub releases)
- No LFS quota concerns
- Easier for contributors

### **For Private/Enterprise Projects:**
✅ **Consider Git LFS** if:
- You have unlimited LFS storage
- You need version control for models
- You want one-step clone
- Your team is familiar with LFS

---

## 🔍 How to Check What's Using LFS

```powershell
# List files tracked by LFS
git lfs ls-files

# Show LFS storage usage
git lfs status

# Check if a file is in LFS
git lfs ls-files | findstr "filename.bin"
```

---

## 🚨 Common Git LFS Issues

### **Issue 1: LFS not installed**
```
Error: git: 'lfs' is not a git command
```
**Solution:** Install Git LFS (see Step 1 above)

### **Issue 2: LFS quota exceeded (GitHub)**
```
Error: batch response: This repository is over its data quota
```
**Solution:**
- Upgrade GitHub plan for more LFS storage
- OR switch to `setup.bat` approach (remove files from LFS)

### **Issue 3: Slow clone due to LFS**
```
Downloading large files...
```
**Solution:**
- Use `git lfs clone` instead of `git clone` (faster LFS downloads)
- OR clone without LFS: `GIT_LFS_SKIP_SMUDGE=1 git clone <url>`

---

## 📝 Current Repository Status

**What's in Git:**
- ✅ Source code (.cpp, .h, .py)
- ✅ CMakeLists.txt, package.json, etc.
- ✅ Documentation (.md files)
- ✅ Small config files

**What's NOT in Git (downloaded by setup.bat):**
- ❌ AI models (`models/`)
- ❌ OpenCV (`windows_code/third-party/opencv/`)
- ❌ ONNX Runtime (`windows_code/third-party/onnxruntime/`)
- ❌ Build artifacts (`windows_code/build/`, `*.exe`)

**Total repository size:** ~5MB (without large files)
**Total with large files:** ~200MB

---

## 💡 Recommendation

**For this project, stick with `setup.bat` approach:**

1. ✅ Keeps repository small (~5MB)
2. ✅ No LFS quota concerns
3. ✅ Downloads from official sources (traceable)
4. ✅ Easy for colleagues to set up
5. ✅ No special Git configuration needed

**Only use Git LFS if:**
- You need to version control models
- You have dedicated LFS storage (GitLab, Bitbucket, or enterprise GitHub)
- Your team is comfortable with LFS

---

## 🔗 Resources

- Git LFS Official Site: https://git-lfs.github.com/
- GitHub LFS Documentation: https://docs.github.com/en/repositories/working-with-files/managing-large-files
- Git LFS Tutorial: https://github.com/git-lfs/git-lfs/wiki/Tutorial

---

**Last Updated:** 2025-10-10
**Recommended Approach:** Use `setup.bat` (keep large files out of Git)
