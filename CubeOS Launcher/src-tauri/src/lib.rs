use dirs::home_dir;
use reqwest::header::{ACCEPT, USER_AGENT};
use serde::{Deserialize, Serialize};
use std::ffi::OsStr;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};
use walkdir::WalkDir;

const GITHUB_RELEASES_URL: &str = "https://api.github.com/repos/amazonka142/CubeOS/releases";

#[derive(Debug, Deserialize)]
struct GitHubRelease {
    tag_name: String,
    name: String,
    body: Option<String>,
    html_url: String,
    published_at: Option<String>,
    draft: bool,
    #[serde(default)]
    prerelease: bool,
    assets: Vec<GitHubReleaseAsset>,
}

#[derive(Debug, Deserialize, Clone)]
struct GitHubReleaseAsset {
    name: String,
    browser_download_url: String,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct LauncherState {
    install_root: String,
    shared_data_root: String,
    releases: Vec<ReleaseSummary>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct ReleaseSummary {
    tag: String,
    title: String,
    notes: String,
    source_url: String,
    published_at: Option<String>,
    macos_asset_name: String,
    asset_format: String,
    is_prerelease: bool,
    installed: bool,
    installed_app_path: Option<String>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct InstallResult {
    tag: String,
    app_name: String,
    app_path: String,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
struct InstallMetadata {
    tag: String,
    asset_name: String,
    download_url: String,
    app_name: String,
    app_path: String,
    installed_at_epoch_secs: u64,
}

fn current_epoch_secs() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs())
        .unwrap_or(0)
}

fn macos_home_support_dir(name: &str) -> Result<PathBuf, String> {
    let home = home_dir().ok_or_else(|| "Could not determine the home directory.".to_string())?;
    Ok(home
        .join("Library")
        .join("Application Support")
        .join(name))
}

fn launcher_root_dir() -> Result<PathBuf, String> {
    macos_home_support_dir("CubeOS Launcher")
}

fn cubeos_shared_data_dir() -> Result<PathBuf, String> {
    macos_home_support_dir("CubeOS")
}

fn versions_dir() -> Result<PathBuf, String> {
    Ok(launcher_root_dir()?.join("versions"))
}

fn downloads_dir() -> Result<PathBuf, String> {
    Ok(launcher_root_dir()?.join("downloads"))
}

fn ensure_launcher_dirs() -> Result<(), String> {
    for dir in [launcher_root_dir()?, versions_dir()?, downloads_dir()?] {
        fs::create_dir_all(&dir)
            .map_err(|error| format!("Failed to create {}: {error}", dir.display()))?;
    }
    Ok(())
}

fn safe_tag(tag: &str) -> String {
    tag.chars()
        .map(|ch| match ch {
            'a'..='z' | 'A'..='Z' | '0'..='9' | '.' | '-' | '_' => ch,
            _ => '_',
        })
        .collect()
}

fn safe_filename(name: &str) -> String {
    name.chars()
        .map(|ch| match ch {
            '/' | '\\' | ':' => '_',
            _ => ch,
        })
        .collect()
}

fn path_string(path: &Path) -> String {
    path.to_string_lossy().into_owned()
}

fn version_install_dir(tag: &str) -> Result<PathBuf, String> {
    Ok(versions_dir()?.join(safe_tag(tag)))
}

fn metadata_path(tag: &str) -> Result<PathBuf, String> {
    Ok(version_install_dir(tag)?.join("install.json"))
}

fn is_supported_macos_asset(asset_name: &str) -> bool {
    let asset_name = asset_name.to_ascii_lowercase();
    asset_name.ends_with(".dmg")
        || asset_name.ends_with(".zip")
        || asset_name.ends_with(".tar.gz")
}

fn asset_format(asset_name: &str) -> String {
    let lower = asset_name.to_ascii_lowercase();
    if lower.ends_with(".dmg") {
        "dmg".to_string()
    } else if lower.ends_with(".tar.gz") {
        "tar.gz".to_string()
    } else if lower.ends_with(".zip") {
        "zip".to_string()
    } else {
        "archive".to_string()
    }
}

fn supported_macos_asset(release: &GitHubRelease) -> Option<&GitHubReleaseAsset> {
    release
        .assets
        .iter()
        .find(|asset| {
            let name = asset.name.to_ascii_lowercase();
            is_supported_macos_asset(&name) && name.starts_with("cubeos-v") && !name.contains("launcher")
        })
}

fn run_command(command: &mut Command, description: &str) -> Result<String, String> {
    let output = command
        .output()
        .map_err(|error| format!("Failed to {description}: {error}"))?;

    if output.status.success() {
        return Ok(String::from_utf8_lossy(&output.stdout).trim().to_string());
    }

    let stderr = String::from_utf8_lossy(&output.stderr).trim().to_string();
    let stdout = String::from_utf8_lossy(&output.stdout).trim().to_string();
    let details = if stderr.is_empty() { stdout } else { stderr };

    Err(if details.is_empty() {
        format!("Failed to {description}: process exited with {}", output.status)
    } else {
        format!("Failed to {description}: {details}")
    })
}

fn find_first_app_bundle(root: &Path) -> Option<PathBuf> {
    WalkDir::new(root)
        .follow_links(false)
        .into_iter()
        .filter_map(Result::ok)
        .find(|entry| {
            entry.file_type().is_dir() && entry.path().extension() == Some(OsStr::new("app"))
        })
        .map(|entry| entry.into_path())
}

fn copy_app_bundle(source_app: &Path, destination_dir: &Path) -> Result<PathBuf, String> {
    fs::create_dir_all(destination_dir).map_err(|error| {
        format!(
            "Failed to prepare install directory {}: {error}",
            destination_dir.display()
        )
    })?;

    let app_name = source_app
        .file_name()
        .ok_or_else(|| "The downloaded release did not include a valid .app bundle.".to_string())?;
    let destination_app = destination_dir.join(app_name);

    if destination_app.exists() {
        fs::remove_dir_all(&destination_app).map_err(|error| {
            format!(
                "Failed to replace existing app bundle {}: {error}",
                destination_app.display()
            )
        })?;
    }

    run_command(
        Command::new("ditto")
            .arg(source_app)
            .arg(&destination_app),
        "copy the app bundle",
    )?;

    Ok(destination_app)
}

fn parse_mount_path(output: &str) -> Option<String> {
    output.lines().rev().find_map(|line| {
        let candidate = line
            .split('\t')
            .next_back()
            .map(str::trim)
            .unwrap_or_default();

        if candidate.starts_with("/Volumes/") {
            Some(candidate.to_string())
        } else {
            None
        }
    })
}

fn install_from_dmg(archive_path: &Path, destination_dir: &Path) -> Result<PathBuf, String> {
    let attach_output = run_command(
        Command::new("hdiutil")
            .arg("attach")
            .arg("-nobrowse")
            .arg("-readonly")
            .arg(archive_path),
        "mount the DMG",
    )?;

    let mount_path = parse_mount_path(&attach_output)
        .ok_or_else(|| "Mounted DMG, but could not find its mount path.".to_string())?;
    let mount_path_buf = PathBuf::from(&mount_path);

    let install_result = (|| {
        let source_app = find_first_app_bundle(&mount_path_buf)
            .ok_or_else(|| "No .app bundle was found inside the mounted DMG.".to_string())?;
        copy_app_bundle(&source_app, destination_dir)
    })();

    let _ = run_command(
        Command::new("hdiutil")
            .arg("detach")
            .arg(&mount_path)
            .arg("-quiet"),
        "unmount the DMG",
    );

    install_result
}

fn install_from_zip(
    archive_path: &Path,
    destination_dir: &Path,
    workspace_dir: &Path,
) -> Result<PathBuf, String> {
    let extract_dir = workspace_dir.join("zip");
    fs::create_dir_all(&extract_dir).map_err(|error| {
        format!(
            "Failed to prepare extraction directory {}: {error}",
            extract_dir.display()
        )
    })?;

    run_command(
        Command::new("ditto")
            .arg("-x")
            .arg("-k")
            .arg(archive_path)
            .arg(&extract_dir),
        "extract the ZIP archive",
    )?;

    let source_app = find_first_app_bundle(&extract_dir)
        .ok_or_else(|| "No .app bundle was found inside the ZIP archive.".to_string())?;
    copy_app_bundle(&source_app, destination_dir)
}

fn install_from_tar_gz(
    archive_path: &Path,
    destination_dir: &Path,
    workspace_dir: &Path,
) -> Result<PathBuf, String> {
    let extract_dir = workspace_dir.join("tar");
    fs::create_dir_all(&extract_dir).map_err(|error| {
        format!(
            "Failed to prepare extraction directory {}: {error}",
            extract_dir.display()
        )
    })?;

    run_command(
        Command::new("tar")
            .arg("-xzf")
            .arg(archive_path)
            .arg("-C")
            .arg(&extract_dir),
        "extract the tar.gz archive",
    )?;

    let source_app = find_first_app_bundle(&extract_dir)
        .ok_or_else(|| "No .app bundle was found inside the tar.gz archive.".to_string())?;
    copy_app_bundle(&source_app, destination_dir)
}

fn write_install_metadata(
    tag: &str,
    asset_name: &str,
    download_url: &str,
    app_path: &Path,
) -> Result<(), String> {
    let app_name = app_path
        .file_stem()
        .unwrap_or_else(|| OsStr::new("CubeOS"))
        .to_string_lossy()
        .into_owned();

    let metadata = InstallMetadata {
        tag: tag.to_string(),
        asset_name: asset_name.to_string(),
        download_url: download_url.to_string(),
        app_name,
        app_path: path_string(app_path),
        installed_at_epoch_secs: current_epoch_secs(),
    };

    let content = serde_json::to_vec_pretty(&metadata)
        .map_err(|error| format!("Failed to serialize install metadata: {error}"))?;
    let metadata_path = metadata_path(tag)?;
    fs::write(&metadata_path, content).map_err(|error| {
        format!(
            "Failed to write install metadata {}: {error}",
            metadata_path.display()
        )
    })?;

    Ok(())
}

fn installed_app_path_for_tag(tag: &str) -> Result<Option<PathBuf>, String> {
    let install_dir = version_install_dir(tag)?;
    if !install_dir.exists() {
        return Ok(None);
    }

    Ok(find_first_app_bundle(&install_dir))
}

async fn github_releases() -> Result<Vec<GitHubRelease>, String> {
    let client = reqwest::Client::new();
    client
        .get(GITHUB_RELEASES_URL)
        .header(ACCEPT, "application/vnd.github+json")
        .header(USER_AGENT, "CubeOS Launcher")
        .send()
        .await
        .map_err(|error| format!("Failed to contact GitHub Releases API: {error}"))?
        .error_for_status()
        .map_err(|error| format!("GitHub Releases API returned an error: {error}"))?
        .json::<Vec<GitHubRelease>>()
        .await
        .map_err(|error| format!("Failed to decode the releases feed: {error}"))
}

async fn resolve_release_asset(tag: &str, asset_name: &str) -> Result<GitHubReleaseAsset, String> {
    let releases = github_releases().await?;
    let release = releases
        .into_iter()
        .filter(|release| !release.draft)
        .find(|release| release.tag_name == tag)
        .ok_or_else(|| format!("Could not find release {tag} on GitHub."))?;

    let asset = release
        .assets
        .iter()
        .find(|asset| asset.name == asset_name)
        .ok_or_else(|| format!("Could not find asset {asset_name} in release {tag}."))?;

    if !is_supported_macos_asset(&asset.name) {
        return Err(format!(
            "Unsupported macOS asset format for {}. Expected .dmg, .zip, or .tar.gz.",
            asset.name
        ));
    }

    Ok(asset.clone())
}

async fn download_archive(download_url: &str, asset_name: &str, tag: &str) -> Result<PathBuf, String> {
    let client = reqwest::Client::new();
    let mut response = client
        .get(download_url)
        .header(USER_AGENT, "CubeOS Launcher")
        .send()
        .await
        .map_err(|error| format!("Failed to download {asset_name}: {error}"))?
        .error_for_status()
        .map_err(|error| format!("Download failed for {asset_name}: {error}"))?;

    let archive_name = format!("{}-{}", safe_tag(tag), safe_filename(asset_name));
    let archive_path = downloads_dir()?.join(&archive_name);
    let partial_path = downloads_dir()?.join(format!("{archive_name}.part"));

    if partial_path.exists() {
        let _ = fs::remove_file(&partial_path);
    }

    let mut file = fs::File::create(&partial_path).map_err(|error| {
        format!(
            "Failed to create the temporary download file {}: {error}",
            partial_path.display()
        )
    })?;

    while let Some(chunk) = response
        .chunk()
        .await
        .map_err(|error| format!("Failed to read the downloaded archive: {error}"))?
    {
        if let Err(error) = file.write_all(&chunk) {
            let _ = fs::remove_file(&partial_path);
            return Err(format!(
                "Failed to write the downloaded archive {}: {error}",
                partial_path.display()
            ));
        }
    }

    if let Err(error) = file.flush() {
        let _ = fs::remove_file(&partial_path);
        return Err(format!(
            "Failed to finalize the downloaded archive {}: {error}",
            partial_path.display()
        ));
    }

    if archive_path.exists() {
        let _ = fs::remove_file(&archive_path);
    }

    fs::rename(&partial_path, &archive_path).map_err(|error| {
        format!(
            "Failed to save the downloaded archive {}: {error}",
            archive_path.display()
        )
    })?;

    Ok(archive_path)
}

#[cfg(target_os = "macos")]
#[tauri::command]
async fn load_launcher_state() -> Result<LauncherState, String> {
    ensure_launcher_dirs()?;

    let releases = github_releases().await?;
    let mut mapped = Vec::new();

    for release in releases.into_iter().filter(|release| !release.draft) {
        let Some(asset) = supported_macos_asset(&release) else {
            continue;
        };
        let asset_name = asset.name.clone();
        let asset_format = asset_format(&asset_name);

        let installed_app_path = installed_app_path_for_tag(&release.tag_name)?;
        mapped.push(ReleaseSummary {
            tag: release.tag_name,
            title: release.name,
            notes: release.body.unwrap_or_default().trim().to_string(),
            source_url: release.html_url,
            published_at: release.published_at,
            macos_asset_name: asset_name,
            asset_format,
            is_prerelease: release.prerelease,
            installed: installed_app_path.is_some(),
            installed_app_path: installed_app_path.as_deref().map(path_string),
        });
    }

    Ok(LauncherState {
        install_root: path_string(&versions_dir()?),
        shared_data_root: path_string(&cubeos_shared_data_dir()?),
        releases: mapped,
    })
}

#[cfg(not(target_os = "macos"))]
#[tauri::command]
async fn load_launcher_state() -> Result<LauncherState, String> {
    Err("This launcher build currently supports macOS only.".to_string())
}

#[cfg(target_os = "macos")]
#[tauri::command]
async fn install_release(tag: String, asset_name: String) -> Result<InstallResult, String> {
    ensure_launcher_dirs()?;

    let asset = resolve_release_asset(&tag, &asset_name).await?;
    let download_url = asset.browser_download_url;
    let archive_path = download_archive(&download_url, &asset_name, &tag).await?;
    let safe_tag = safe_tag(&tag);
    let versions_root = versions_dir()?;
    let final_install_dir = versions_root.join(&safe_tag);
    let staging_dir = versions_root.join(format!(".{}.staging-{}", safe_tag, current_epoch_secs()));
    let workspace_dir = launcher_root_dir()?.join("tmp").join(format!("{safe_tag}-{}", current_epoch_secs()));

    if staging_dir.exists() {
        let _ = fs::remove_dir_all(&staging_dir);
    }
    fs::create_dir_all(&staging_dir).map_err(|error| {
        format!(
            "Failed to prepare the staging directory {}: {error}",
            staging_dir.display()
        )
    })?;
    fs::create_dir_all(&workspace_dir).map_err(|error| {
        format!(
            "Failed to prepare the temporary workspace {}: {error}",
            workspace_dir.display()
        )
    })?;

    let staged_app_path = if asset_name.to_ascii_lowercase().ends_with(".dmg") {
        install_from_dmg(&archive_path, &staging_dir)
    } else if asset_name.to_ascii_lowercase().ends_with(".zip") {
        install_from_zip(&archive_path, &staging_dir, &workspace_dir)
    } else {
        install_from_tar_gz(&archive_path, &staging_dir, &workspace_dir)
    };

    let staged_app_path = match staged_app_path {
        Ok(path) => path,
        Err(error) => {
            let _ = fs::remove_dir_all(&staging_dir);
            let _ = fs::remove_dir_all(&workspace_dir);
            return Err(error);
        }
    };

    let _ = fs::remove_dir_all(&workspace_dir);

    if final_install_dir.exists() {
        fs::remove_dir_all(&final_install_dir).map_err(|error| {
            format!(
                "Failed to replace the existing install {}: {error}",
                final_install_dir.display()
            )
        })?;
    }

    fs::rename(&staging_dir, &final_install_dir).map_err(|error| {
        format!(
            "Failed to move the staged install into {}: {error}",
            final_install_dir.display()
        )
    })?;

    let final_app_path = final_install_dir.join(
        staged_app_path
            .file_name()
            .ok_or_else(|| "The staged app bundle had no file name.".to_string())?,
    );

    write_install_metadata(&tag, &asset_name, &download_url, &final_app_path)?;

    Ok(InstallResult {
        tag,
        app_name: final_app_path
            .file_stem()
            .unwrap_or_else(|| OsStr::new("CubeOS"))
            .to_string_lossy()
            .into_owned(),
        app_path: path_string(&final_app_path),
    })
}

#[cfg(not(target_os = "macos"))]
#[tauri::command]
async fn install_release(_tag: String, _asset_name: String) -> Result<InstallResult, String> {
    Err("This launcher build currently supports macOS only.".to_string())
}

#[cfg(target_os = "macos")]
#[tauri::command]
fn launch_release(tag: String) -> Result<(), String> {
    let Some(app_path) = installed_app_path_for_tag(&tag)? else {
        return Err(format!("{tag} is not installed yet."));
    };

    run_command(
        Command::new("open").arg(&app_path),
        "launch the installed CubeOS build",
    )?;

    Ok(())
}

#[cfg(not(target_os = "macos"))]
#[tauri::command]
fn launch_release(_tag: String) -> Result<(), String> {
    Err("This launcher build currently supports macOS only.".to_string())
}

#[cfg(target_os = "macos")]
#[tauri::command]
fn open_versions_directory() -> Result<String, String> {
    ensure_launcher_dirs()?;
    let path = versions_dir()?;
    run_command(Command::new("open").arg(&path), "open the versions folder")?;
    Ok(path_string(&path))
}

#[cfg(not(target_os = "macos"))]
#[tauri::command]
fn open_versions_directory() -> Result<String, String> {
    Err("This launcher build currently supports macOS only.".to_string())
}

#[cfg(target_os = "macos")]
#[tauri::command]
fn open_shared_data_directory() -> Result<String, String> {
    let path = cubeos_shared_data_dir()?;
    fs::create_dir_all(&path)
        .map_err(|error| format!("Failed to prepare {}: {error}", path.display()))?;
    run_command(Command::new("open").arg(&path), "open the shared CubeOS data folder")?;
    Ok(path_string(&path))
}

#[cfg(not(target_os = "macos"))]
#[tauri::command]
fn open_shared_data_directory() -> Result<String, String> {
    Err("This launcher build currently supports macOS only.".to_string())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            load_launcher_state,
            install_release,
            launch_release,
            open_versions_directory,
            open_shared_data_directory
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
